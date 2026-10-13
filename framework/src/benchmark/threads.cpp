#include "benchmark/threads.h"
#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "benchmark/requests.h"
#include "analysis/stats.h"
#include "logging/logger.h"

#include <chrono>
#include <cstddef>
#include <random>
#include <system_error>
#include <thread>
#include <utility>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    ClientReport::~ClientReport() {
        if (latency_us.histogram != nullptr) {
            analysis::RunningStatsDestroy(latency_us);
        }
    }

    ClientReport::ClientReport(ClientReport&& other) noexcept
        : client_id(other.client_id), step_idx(other.step_idx), reason(other.reason),
          interval_start(other.interval_start), interval_end(other.interval_end), latency_us(other.latency_us) {
        other.latency_us.histogram = nullptr;
    }

    ClientReport& ClientReport::operator=(ClientReport&& other) noexcept {
        if (this == &other) { return *this; }

        if (latency_us.histogram != nullptr) {
            analysis::RunningStatsDestroy(latency_us);
        }

        client_id = other.client_id;
        step_idx = other.step_idx;
        reason = other.reason;
        interval_start = other.interval_start;
        interval_end = other.interval_end;
        latency_us = other.latency_us;
        other.latency_us.histogram = nullptr;
        return *this;
    }

    BenchmarkThread::BenchmarkThread(
        size_t client_id,
        const BenchmarkContext* ctx,
        const Sequence* sequence,
        Channel<ClientSignal>* signals,
        Channel<ClientReport>* reports,
        logging::Logger* logger,
        std::chrono::milliseconds report_interval
    ) : client_id(client_id), ctx(ctx), sequence(sequence), signals(signals), reports(reports), logger(logger),
        report_interval(report_interval), rng(std::random_device{}()), latency_stats{nullptr},
        failed(false), stop_requested(false) {}

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
        if (!analysis::RunningStatsInit(latency_stats)) {
            logger->error("client %zu: failed to allocate latency histogram", client_id);
            failed = true;
            SendFinal(0, ReportReason::FAILED);
            return;
        }

        if (!RunSequence()) {
            failed = true;
        }

        analysis::RunningStatsDestroy(latency_stats);
    }

    bool BenchmarkThread::RunSequence() {
        // A local subchannel pool keeps gRPC from sharing one connection between all the clients
        grpc::ChannelArguments channel_args;
        channel_args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
        auto channel = grpc::CreateCustomChannel(ctx->server_address, grpc::InsecureChannelCredentials(), channel_args);
        auto stub = inference::GRPCInferenceService::NewStub(channel);

        size_t step_idx = 0;
        for (; step_idx < sequence->steps.size() && !stop_requested; step_idx++) {
            const Step& step = sequence->steps[step_idx];

            auto spec_it = ctx->model_directory.find(step.model_name);
            if (spec_it == ctx->model_directory.end()) {
                logger->error("client %zu: model \"%s\" for step %zu is not in the model directory", client_id, step.model_name.c_str(), step_idx);
                SendFinal(step_idx, ReportReason::FAILED);
                return false;
            }

            analysis::RunningStatsClear(latency_stats);
            interval_start = std::chrono::steady_clock::now();

            auto hook = [this, step_idx](std::chrono::steady_clock::time_point now) {
                bool keep_going = true;
                if (!PollSignals(step_idx, now, keep_going)) { return false; }

                if (report_interval.count() > 0 && now - interval_start >= report_interval) {
                    if (!SendReport(step_idx, ReportReason::INTERVAL, now)) { return false; }
                }
                return keep_going;
            };

            if (!RunRequestLoop(stub.get(), spec_it->second, step, rng, latency_stats, logger, hook) || failed) {
                logger->error("client %zu: step %zu (model \"%s\", batch size %d) failed", client_id, step_idx, step.model_name.c_str(), step.batch_size);
                SendFinal(step_idx, ReportReason::FAILED);
                return false;
            }

            SendReport(step_idx, ReportReason::STEP_END, std::chrono::steady_clock::now());
            if (failed) {
                SendFinal(step_idx, ReportReason::FAILED);
                return false;
            }
        }

        SendFinal(step_idx > 0 ? step_idx - 1 : 0, ReportReason::FINISHED);
        return true;
    }

    // Handles all pending signals for the current step. Sets keep_going to false on STOP/NEXT.
    // Returns false if the loop can't continue (a report couldn't be sent).
    bool BenchmarkThread::PollSignals(size_t step_idx, std::chrono::steady_clock::time_point now, bool& keep_going) {
        ClientSignal signal;
        while (signals->try_receive(signal)) {
            switch (signal) {
                case ClientSignal::REPORT:
                    if (!SendReport(step_idx, ReportReason::REQUESTED, now)) { return false; }
                    break;
                case ClientSignal::NEXT:
                    keep_going = false;
                    return true;
                case ClientSignal::STOP:
                    stop_requested = true;
                    keep_going = false;
                    return true;
            }
        }

        if (signals->is_closed()) {
            stop_requested = true;
            keep_going = false;
        }
        return true;
    }

    // Hands the current histogram off in a report and starts a fresh one for the next interval.
    bool BenchmarkThread::SendReport(size_t step_idx, ReportReason reason, std::chrono::steady_clock::time_point now) {
        analysis::RunningStats fresh = {nullptr};
        if (!analysis::RunningStatsInit(fresh)) {
            logger->error("client %zu: failed to allocate latency histogram", client_id);
            failed = true;
            return false;
        }

        ClientReport report;
        report.client_id = client_id;
        report.step_idx = step_idx;
        report.reason = reason;
        report.interval_start = interval_start;
        report.interval_end = now;
        report.latency_us = latency_stats;

        latency_stats = fresh;
        interval_start = now;

        if (!reports->send(std::move(report))) {
            logger->warning("client %zu: report channel closed, stopping", client_id);
            stop_requested = true;
            return false;
        }
        return true;
    }

    void BenchmarkThread::SendFinal(size_t step_idx, ReportReason reason) {
        ClientReport report;
        report.client_id = client_id;
        report.step_idx = step_idx;
        report.reason = reason;
        report.interval_start = std::chrono::steady_clock::now();
        report.interval_end = report.interval_start;

        reports->send(std::move(report));
    }

} // namespace benchmark
