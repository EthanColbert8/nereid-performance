#pragma once

#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "analysis/stats.h"
#include "logging/logger.h"
// #include "benchmark/signals.h" // Channel, ClientSignal, ClientReport: kept for the SIGNAL stop condition later

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    // One-shot gate that lets every client in a stage start sending requests at the same moment.
    // Opens once `expected` threads have arrived, or when Release() is called (e.g. a thread failed
    // to start and will never arrive). Once open it stays open.
    class StartGate {
        private:
            std::mutex mtx;
            std::condition_variable cv;
            size_t expected;
            size_t arrived;
            bool open;

        public:
            explicit StartGate(size_t expected);
            StartGate(const StartGate&) = delete;
            StartGate& operator=(const StartGate&) = delete;

            void ArriveAndWait();
            void Release();
    };

    // Results of one completed step for one client. Owns its latency histogram (microseconds).
    // Throughput can be computed as num_requests * batch_size / (end - start).
    struct StepResult {
        size_t step_idx = 0;
        std::string start_timestamp; // wall clock, same format as the logs
        std::chrono::steady_clock::time_point start; // just before the first request
        std::chrono::steady_clock::time_point end;   // when the last response was received
        int64_t num_requests = 0;
        analysis::RunningStats latency_us = {nullptr};

        StepResult() = default;
        ~StepResult();
        StepResult(StepResult&& other) noexcept;
        StepResult& operator=(StepResult&& other) noexcept;
        StepResult(const StepResult&) = delete;
        StepResult& operator=(const StepResult&) = delete;
    };

    // Runs one client's request sequence on its own thread with its own connection to the server.
    //
    // The thread connects, waits at the start gate, then runs each step and records a StepResult.
    // The gate and abort flag are owned by the caller, shared by all clients in the stage, and must
    // outlive the thread. If this client fails it sets the abort flag; if it sees the abort flag set
    // it stops early without recording the step in progress.
    //
    // Results() and StoppedEarly() may only be read after Join() returns (join provides the
    // synchronization). If Start() fails, the caller must set the abort flag and Release() the gate,
    // or the other clients will wait at the gate forever.
    class BenchmarkThread {
        private:
            size_t client_id;
            const BenchmarkContext* ctx;
            const Sequence* sequence;
            StartGate* gate;
            std::atomic<bool>* abort;
            logging::Logger* logger;

            // NOTE (Ethan): signalling machinery, unused until the SIGNAL stop condition is implemented
            // Channel<ClientSignal>* signals;
            // Channel<ClientReport>* reports;
            // std::chrono::milliseconds report_interval; // zero disables periodic reports
            // analysis::RunningStats latency_stats; // histogram for the current report interval
            // std::chrono::steady_clock::time_point interval_start;
            // bool stop_requested;

            std::thread thread;
            std::mt19937 rng;
            std::vector<StepResult> results;
            std::atomic<bool> failed;
            bool stopped_early; // stopped because another client failed (written by the thread only)

            void Run();
            bool RunSequence();
            bool StopIfAborted(const char* when);

            // NOTE (Ethan): signalling machinery, unused until the SIGNAL stop condition is implemented
            // bool PollSignals(size_t step_idx, std::chrono::steady_clock::time_point now, bool& keep_going);
            // bool SendReport(size_t step_idx, ReportReason reason, std::chrono::steady_clock::time_point now);
            // void SendFinal(size_t step_idx, ReportReason reason);

        public:
            BenchmarkThread(
                size_t client_id,
                const BenchmarkContext* ctx,
                const Sequence* sequence,
                StartGate* gate,
                std::atomic<bool>* abort,
                logging::Logger* logger
                // NOTE (Ethan): signalling machinery, unused until the SIGNAL stop condition is implemented
                // Channel<ClientSignal>* signals,
                // Channel<ClientReport>* reports,
                // std::chrono::milliseconds report_interval = std::chrono::milliseconds(0)
            );
            ~BenchmarkThread(); // joins the thread if it is still running

            BenchmarkThread(const BenchmarkThread&) = delete;
            BenchmarkThread& operator=(const BenchmarkThread&) = delete;

            bool Start();
            void Join();
            bool Failed() const { return failed.load(); }
            size_t ClientId() const { return client_id; }

            // Only valid after Join()
            bool StoppedEarly() const { return stopped_early; }
            const std::vector<StepResult>& Results() const { return results; }
    };

} // namespace benchmark
