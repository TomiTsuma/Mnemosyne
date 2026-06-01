// src/Common/thread_pool.cpp — ThreadPool implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "thread_pool.h"

namespace mnesso::common {

namespace {
    size_t default_thread_count() {
        auto n = std::thread::hardware_concurrency();
        return n > 0 ? n : 4;
    }
} // namespace

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = default_thread_count();
    }
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;
        {
            std::unique_lock lock{queue_mutex_};
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        active_.fetch_add(1);
        task();
        active_.fetch_sub(1);
        pending_.fetch_sub(1);
    }
}

void ThreadPool::wait_all() {
    while (true) {
        {
            std::lock_guard lock{queue_mutex_};
            if (tasks_.empty() && active_.load() == 0) return;
        }
        std::this_thread::yield();
    }
}

void ThreadPool::shutdown() {
    stop_.store(true);
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

size_t ThreadPool::size()       const { return workers_.size(); }
size_t ThreadPool::active_count() const { return active_.load(); }
size_t ThreadPool::queue_size()   const { return pending_.load(); }

} // namespace mnesso::common
