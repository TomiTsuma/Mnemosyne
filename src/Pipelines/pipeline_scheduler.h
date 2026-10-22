// src/Pipelines/pipeline_scheduler.h — Background trigger scheduler (cron and events)

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string_view>
#include <thread>

namespace mnemo::interpreters {
class Context;
}

namespace mnemo::pipelines {

/// Runs pipelines when their triggers fire: schedule triggers when their cron expression matches
/// (checked every tick), and event triggers (table insert, stream event, webhook) as soon as the
/// PipelineEventBus has queued them. Both go through the same run callback.
class PipelineScheduler {
public:
    using RunCallback = std::function<void(std::string_view pipeline_name,
                                           std::string_view trigger_name)>;

    static auto instance() -> PipelineScheduler&;

    void set_run_callback(RunCallback callback);
    void start(std::chrono::seconds tick_interval = std::chrono::seconds{5});
    void stop();
    [[nodiscard]] auto is_running() const -> bool;
    /// Fires every schedule trigger whose cron expression matches now.
    void tick();
    /// Runs the pipelines of every queued event trigger. A trigger whose pipeline was dropped, is
    /// paused or is already running is dropped, like a missed schedule.
    void dispatch_events();
    /// Makes the worker look at the event queue now instead of at the next tick.
    void wake();

private:
    PipelineScheduler() = default;

    std::atomic<bool> running_{false};
    std::chrono::seconds tick_interval_{5};
    std::thread tick_thread_;
    RunCallback run_callback_;

    std::mutex wake_mutex_;
    std::condition_variable wake_cv_;
    bool wake_requested_ = false;
};

} // namespace mnemo::pipelines
