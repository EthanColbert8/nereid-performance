#pragma once

// Signalling machinery for talking to running client threads: a bounded channel, the signals a
// controller can send, and the latency reports a client can send back.
//
// NOTE (Ethan): Not used yet, and intentionally not included anywhere, so none of this is compiled
//               into the executable. It's kept as one possible direction for the SIGNAL stop
//               condition. The matching pieces of BenchmarkThread are commented out in threads.h
//               and threads.cpp.

#include "analysis/stats.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

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
        START,  // begin the sequence (not handled yet; the StartGate in threads.h covers this for now)
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

        ~ClientReport() {
            if (latency_us.histogram != nullptr) {
                analysis::RunningStatsDestroy(latency_us);
            }
        }

        ClientReport(ClientReport&& other) noexcept
            : client_id(other.client_id), step_idx(other.step_idx), reason(other.reason),
              interval_start(other.interval_start), interval_end(other.interval_end), latency_us(other.latency_us) {
            other.latency_us.histogram = nullptr;
        }

        ClientReport& operator=(ClientReport&& other) noexcept {
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

        ClientReport(const ClientReport&) = delete;
        ClientReport& operator=(const ClientReport&) = delete;
    };

} // namespace benchmark
