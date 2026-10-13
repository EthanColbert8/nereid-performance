#include "benchmark/threads.h"

#include <deque>
#include <condition_variable>
#include <mutex>

namespace benchmark {

    bool Channel<T>::send(T value) {
        std::unique_lock lock(mtx);

        // if (closed) { return false; }
        not_full.wait(lock, [&] { return !full() || closed; });
        if (closed) { return false; }

        queue.push_back(std::move(value));
        lock.unlock();

        not_empty.notify_one();
        return true;
    }

    bool Channel<T>::receive(T& item) {
        std::unique_lock lock(mtx);

        not_empty.wait(lock, [&] { return !queue.empty() || closed; });
        return pop(item, lock);
    }

    bool Channel<T>::try_receive(T& item) {
        std::unique_lock lock(mtx);
        return pop(item, lock);
    }

    void Channel<T>::close() {
        {
            std::lock_guard lock(mtx);
            closed = true;
        }

        not_empty.notify_all();
        not_full.notify_all();
    }

    bool Channel<T>::full() {
        return queue.size() >= capacity;
    }

    bool Channel<T>::pop(T& item, std::unique_lock<std::mutex>& lock) {
        if (queue.empty()) { return false; }

        item = std::move(queue.front());
        queue.pop_front();
        
        not_full.notify_one();
        return true;
    }

} // namespace benchmark
