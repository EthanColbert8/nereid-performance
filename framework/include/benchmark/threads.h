#pragma once

#include <deque>
#include <condition_variable>
#include <mutex>

namespace benchmark {

    template <typename T>
    class Channel {
        private:
            std::mutex mtx;
            std::condition_variable not_empty;
            std::condition_variable not_full;
            std::deque<T> queue;
            std::size_t capacity;
            bool closed;

            bool full();
            bool pop(T& item);

        public:
            explicit Channel(std::size_t capacity);
            Channel(const Channel&) = delete;
            Channel& operator=(const Channel&) = delete;

            void close();

            bool send(T value);
            bool receive(T& item);
            bool try_receive(T& item);
            
    };

    class BenchmarkThread {
        private:


    };

} // namespace benchmark
