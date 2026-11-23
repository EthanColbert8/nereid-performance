#include "benchmark/threads.h"
#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "benchmark/requests.h"
#include "analysis/stats.h"
#include "logging/logger.h"
#include "utils/timestamp.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <random>
#include <system_error>
#include <thread>
#include <utility>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    // How long a client waits for its connection to the server before giving up
    constexpr std::chrono::seconds CLIENT_CONNECT_TIMEOUT(10);

    // ---- StartGate ----

    StartGate::StartGate(size_t expected) : expected(expected), arrived(0), open(expected == 0) {}

    void StartGate::ArriveAndWait() {
        std::unique_lock lock(mtx);

        arrived++;
        if (arrived >= expected && !open) {
            open = true;
            lock.unlock();
            cv.notify_all();
            return;
        }

        cv.wait(lock, [this] { return open; });
    }

    void StartGate::Release() {
        {
            std::lock_guard lock(mtx);
            open = true;
        }
        cv.notify_all();
    }

    // ---- StepResult ----

    StepResult::~StepResult() {
        if (latency_us.histogram != nullptr) {
            analysis::RunningStatsDestroy(latency_us);
        }
    }

    StepResult::StepResult(StepResult&& other) noexcept
        : step_idx(other.step_idx), start_timestamp(std::move(other.start_timestamp)), start(other.start),
          end(other.end), num_requests(other.num_requests), latency_us(other.latency_us) {
        other.latency_us.histogram = nullptr;
    }

    StepResult& StepResult::operator=(StepResult&& other) noexcept {
        if (this == &other) { return *this; }

        if (latency_us.histogram != nullptr) {
            analysis::RunningStatsDestroy(latency_us);
        }

        step_idx = other.step_idx;
        start_timestamp = std::move(other.start_timestamp);
        start = other.start;
        end = other.end;
        num_requests = other.num_requests;
        latency_us = other.latency_us;
        other.latency_us.histogram = nullptr;
        return *this;
    }

    // ---- BenchmarkThread ----

    BenchmarkThread::BenchmarkThread(
        size_t client_id,
        const BenchmarkContext* ctx,
        const Sequence* sequence,
        StartGate* gate,
        std::atomic<bool>* abort,
        logging::Logger* logger
    ) : client_id(client_id), ctx(ctx), sequence(sequence), gate(gate), abort(abort), logger(logger),
        rng(std::random_device{}()), failed(false), stopped_early(false) {}

    BenchmarkThread::~BenchmarkThread() {
        Join();
    }

    bool BenchmarkThread::Start() {
        if (thread.joinable()) {
            logger->error("client %zu: thread already started", client_id);
            return false;
        }

        try {
            thread = std::thread(&BenchmarkThread::Run, this);
        }
        catch (const std::system_error& e) {
            logger->error("client %zu: failed to start thread: %s", client_id, e.what());
            return false;
        }
        return true;
    }

    void BenchmarkThread::Join() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    void BenchmarkThread::Run() {
        if (!RunSequence()) {
            failed = true;
            abort->store(true);
        }
    }

    // Returns true (and marks this client as stopped early) if another client has failed.
    bool BenchmarkThread::StopIfAborted(const char* when) {
        if (!abort->load(std::memory_order_relaxed)) { return false; }

        stopped_early = true;
        logger->info("client %zu: stopping %s because another client failed", client_id, when);
        return true;
    }

    // Returns false only if this client failed. Stopping early because of the abort flag is not a failure.
    bool BenchmarkThread::RunSequence() {
        // A local subchannel pool keeps gRPC from sharing one connection between all the clients
        grpc::ChannelArguments channel_args;
        channel_args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
        auto channel = grpc::CreateCustomChannel(ctx->server_address, grpc::InsecureChannelCredentials(), channel_args);
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        // Connect before the gate so that connection setup isn't counted in the first request's latency.
        // Every client must arrive at the gate, even on failure, or the others would wait forever.
        const bool connected = channel->WaitForConnected(std::chrono::system_clock::now() + CLIENT_CONNECT_TIMEOUT);
        if (!connected) {
            logger->error("client %zu: could not connect to server at \"%s\"", client_id, ctx->server_address.c_str());
            abort->store(true);
        }

        gate->ArriveAndWait();

        if (!connected) { return false; }

        results.reserve(sequence->steps.size());

        for (size_t step_idx = 0; step_idx < sequence->steps.size(); step_idx++) {
            if (StopIfAborted("before step")) { return true; }

            const Step& step = sequence->steps[step_idx];

            auto spec_it = ctx->model_directory.find(step.model_name);
            if (spec_it == ctx->model_directory.end()) {
                logger->error("client %zu: model \"%s\" for step %zu is not in the model directory", client_id, step.model_name.c_str(), step_idx);
                return false;
            }

            StepResult result;
            result.step_idx = step_idx;
            if (!analysis::RunningStatsInit(result.latency_us)) {
                logger->error("client %zu: failed to allocate latency histogram", client_id);
                return false;
            }

            char timestamp[32];
            utils::FormatTimestamp(timestamp, sizeof(timestamp));
            result.start_timestamp = timestamp;

            auto hook = [this](std::chrono::steady_clock::time_point) {
                return !abort->load(std::memory_order_relaxed);
            };

            // NOTE (Ethan): signal-driven hook, for when the SIGNAL stop condition is implemented
            // analysis::RunningStatsClear(latency_stats);
            // interval_start = std::chrono::steady_clock::now();
            //
            // auto hook = [this, step_idx](std::chrono::steady_clock::time_point now) {
            //     bool keep_going = true;
            //     if (!PollSignals(step_idx, now, keep_going)) { return false; }
            //
            //     if (report_interval.count() > 0 && now - interval_start >= report_interval) {
            //         if (!SendReport(step_idx, ReportReason::INTERVAL, now)) { return false; }
            //     }
            //     return keep_going;
            // };

            RequestLoopSummary summary;
            if (!RunRequestLoop(stub.get(), spec_it->second, step, rng, result.latency_us, summary, logger, hook)) {
                logger->error("client %zu: step %zu (model \"%s\", batch size %d) failed", client_id, step_idx, step.model_name.c_str(), step.batch_size);
                return false;
            }

            // A step cut short by another client's failure is incomplete, so it isn't recorded
            if (StopIfAborted("during step")) { return true; }

            result.start = summary.start;
            result.end = summary.end;
            result.num_requests = summary.num_requests;
            results.push_back(std::move(result));
        }

        return true;
    }

    // NOTE (Ethan): signalling machinery, unused until the SIGNAL stop condition is implemented.
    //               See benchmark/signals.h for Channel, ClientSignal, ReportReason and ClientReport.
    //
    // // Handles all pending signals for the current step. Sets keep_going to false on STOP/NEXT.
    // // Returns false if the loop can't continue (a report couldn't be sent).
    // bool BenchmarkThread::PollSignals(size_t step_idx, std::chrono::steady_clock::time_point now, bool& keep_going) {
    //     ClientSignal signal;
    //     while (signals->try_receive(signal)) {
    //         switch (signal) {
    //             case ClientSignal::REPORT:
    //                 if (!SendReport(step_idx, ReportReason::REQUESTED, now)) { return false; }
    //                 break;
    //             case ClientSignal::NEXT:
    //                 keep_going = false;
    //                 return true;
    //             case ClientSignal::STOP:
    //                 stop_requested = true;
    //                 keep_going = false;
    //                 return true;
    //         }
    //     }
    //
    //     if (signals->is_closed()) {
    //         stop_requested = true;
    //         keep_going = false;
    //     }
    //     return true;
    // }
    //
    // // Hands the current histogram off in a report and starts a fresh one for the next interval.
    // bool BenchmarkThread::SendReport(size_t step_idx, ReportReason reason, std::chrono::steady_clock::time_point now) {
    //     analysis::RunningStats fresh = {nullptr};
    //     if (!analysis::RunningStatsInit(fresh)) {
    //         logger->error("client %zu: failed to allocate latency histogram", client_id);
    //         failed = true;
    //         return false;
    //     }
    //
    //     ClientReport report;
    //     report.client_id = client_id;
    //     report.step_idx = step_idx;
    //     report.reason = reason;
    //     report.interval_start = interval_start;
    //     report.interval_end = now;
    //     report.latency_us = latency_stats;
    //
    //     latency_stats = fresh;
    //     interval_start = now;
    //
    //     if (!reports->send(std::move(report))) {
    //         logger->warning("client %zu: report channel closed, stopping", client_id);
    //         stop_requested = true;
    //         return false;
    //     }
    //     return true;
    // }
    //
    // void BenchmarkThread::SendFinal(size_t step_idx, ReportReason reason) {
    //     ClientReport report;
    //     report.client_id = client_id;
    //     report.step_idx = step_idx;
    //     report.reason = reason;
    //     report.interval_start = std::chrono::steady_clock::now();
    //     report.interval_end = report.interval_start;
    //
    //     reports->send(std::move(report));
    // }

} // namespace benchmark
