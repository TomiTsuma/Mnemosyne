#pragma once

#include <queue>
#include <thread>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>

namespace mnesso::common {

// ── Task functor — anything callable with void() ──
using Task = std::function<void()>;

// ── ThreadPool — work-stealing style with bounded queue ──
class ThreadPool {
public:
    // Create pool with `num_threads` workers (0 = hardware_concurrency)
    explicit ThreadPool(size_t num_threads = 0);
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task — returns a std::promise::future for the result
    template<typename F>
    auto submit(F&& f) -> std::future<std::invoke_result_t<std::remove_cvref_t<F>>> {
        using result_type = std::invoke_result_t<std::remove_cvref_t<F>>;
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::forward<F>(f));
        auto future = task->get_future();
        {
            std::lock_guard lock{queue_mutex_};
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    // Wait until all submitted tasks are done
    void wait_all();

    // Stop the pool — all workers will exit after finishing pending tasks
    void shutdown();

    // Current pool stats
    [[nodiscard]] size_t active_count() const;
    [[nodiscard]] size_t queue_size()   const;

private:
    void worker_loop();

    std::vector<std::thread>        workers_;
    std::queue<Task>                tasks_;
    std::mutex                      queue_mutex_;
    std::condition_variable         cv_;
    std::atomic<bool>               stop_ = false;
    std::atomic<size_t>             active_ = 0;
    std::atomic<size_t>             pending_ = 0;
};

} // namespace mnesso::common
