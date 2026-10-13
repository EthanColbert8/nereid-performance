#pragma once

#include "benchmark/context.h"
#include "benchmark/desc.h"
#include "analysis/stats.h"
#include "logging/logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <random>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "grpc_service.pb.h"

namespace benchmark {

    // Bounded multi-producer/multi-consumer queue. send() blocks while full, receive()
    // blocks while empty. After close(), sends fail and receives drain what's left.
    template <typename T>
    class Channel {
        private:
            std::mutex mtx;
            std::condition_variable not_empty;
            std::condition_variable not_full;
            std::deque<T> queue;
            std::size_t capacity;
            bool closed;

            bool full() const { return queue.size() >= capacity; }

            // mtx must be held
            bool pop(T& item) {
                if (queue.empty()) { return false; }

                item = std::move(queue.front());
                queue.pop_front();

                not_full.notify_one();
                return true;
            }

        public:
            explicit Channel(std::size_t capacity) : capacity(capacity > 0 ? capacity : 1), closed(false) {}
            Channel(const Channel&) = delete;
            Channel& operator=(const Channel&) = delete;

            void close() {
                {
                    std::lock_guard lock(mtx);
                    closed = true;
                }

                not_empty.notify_all();
                not_full.notify_all();
            }

            bool is_closed() {
                std::lock_guard lock(mtx);
                return closed;
            }

            bool send(T value) {
                std::unique_lock lock(mtx);

                not_full.wait(lock, [&] { return !full() || closed; });
                if (closed) { return false; }

                queue.push_back(std::move(value));
                lock.unlock();

                not_empty.notify_one();
                return true;
            }

            // Returns false only once the channel is closed and drained.
            bool receive(T& item) {
                std::unique_lock lock(mtx);

                not_empty.wait(lock, [&] { return !queue.empty() || closed; });
                return pop(item);
            }

            // Returns false if nothing is queued (whether or not the channel is closed).
            bool try_receive(T& item) {
                std::lock_guard lock(mtx);
                return pop(item);
            }
    };

    enum class ClientSignal {
        STOP,   // end the current step and the rest of the sequence
        NEXT,   // end the current step and move on to the next one
        REPORT, // send the latency histogram collected since the last report
    };

    enum class ReportReason {
        INTERVAL,  // periodic report (report_interval elapsed)
        REQUESTED, // response to ClientSignal::REPORT
        STEP_END,  // the step ended; holds everything since the last report
        FINISHED,  // the client is exiting normally; no histogram
        FAILED,    // the client is exiting on an error; no histogram
    };

    // A latency histogram (microseconds) covering requests completed in
    // [interval_start, interval_end]. Owns its histogram, which is null for FINISHED/FAILED.
    // Throughput can be computed as total_count * batch_size / (interval_end - interval_start).
    struct ClientReport {
        size_t client_id = 0;
        size_t step_idx = 0;
        ReportReason reason = ReportReason::INTERVAL;
        std::chrono::steady_clock::time_point interval_start;
        std::chrono::steady_clock::time_point interval_end;
        analysis::RunningStats latency_us = {nullptr};

        ClientReport() = default;
        ~ClientReport();
        ClientReport(ClientReport&& other) noexcept;
        ClientReport& operator=(ClientReport&& other) noexcept;
        ClientReport(const ClientReport&) = delete;
        ClientReport& operator=(const ClientReport&) = delete;
    };

    // Runs one client's request sequence on its own thread with its own connection to the server.
    // The channels are owned by the caller and must outlive the thread; the report channel may be
    // shared between threads (reports carry client_id). A closed signal channel is treated as STOP.
    // The caller must keep draining the report channel, or the thread will block when it fills up.
    class BenchmarkThread {
        private:
            size_t client_id;
            const BenchmarkContext* ctx;
            const Sequence* sequence;
            Channel<ClientSignal>* signals;
            Channel<ClientReport>* reports;
            logging::Logger* logger;
            std::chrono::milliseconds report_interval; // zero disables periodic reports

            std::thread thread;
            std::mt19937 rng;
            analysis::RunningStats latency_stats;
            std::chrono::steady_clock::time_point interval_start;
            std::atomic<bool> failed;
            bool stop_requested;

            void Run();
            bool RunSequence();
            bool PollSignals(size_t step_idx, std::chrono::steady_clock::time_point now, bool& keep_going);
            bool SendReport(size_t step_idx, ReportReason reason, std::chrono::steady_clock::time_point now);
            void SendFinal(size_t step_idx, ReportReason reason);

        public:
            BenchmarkThread(
                size_t client_id,
                const BenchmarkContext* ctx,
                const Sequence* sequence,
                Channel<ClientSignal>* signals,
                Channel<ClientReport>* reports,
                logging::Logger* logger,
                std::chrono::milliseconds report_interval = std::chrono::milliseconds(0)
            );
            ~BenchmarkThread(); // joins the thread if it is still running

            BenchmarkThread(const BenchmarkThread&) = delete;
            BenchmarkThread& operator=(const BenchmarkThread&) = delete;

            bool Start();
            void Join();
            bool Failed() const { return failed.load(); }
            size_t ClientId() const { return client_id; }
    };

} // namespace benchmark
