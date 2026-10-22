// src/Pipelines/pipeline_scheduler.cpp

#include "Pipelines/pipeline_scheduler.h"
#include "Pipelines/cron_matcher.h"
#include "Pipelines/pipeline_event_bus.h"
#include "Pipelines/pipeline_manager.h"
#include <thread>

namespace mnemo::pipelines {

auto PipelineScheduler::instance() -> PipelineScheduler& {
    static PipelineScheduler inst;
    return inst;
}

void PipelineScheduler::set_run_callback(RunCallback callback) {
    run_callback_ = std::move(callback);
}

void PipelineScheduler::tick() {
    if (!run_callback_) return;
    const auto now = std::chrono::system_clock::now();
    auto& mgr = PipelineManager::instance();
    for (const auto& pipeline : mgr.list_pipelines()) {
        if (pipeline.status == PipelineStatus::Paused ||
            pipeline.status == PipelineStatus::Running) {
            continue;
        }
        for (const auto& trigger : pipeline.triggers) {
            if (trigger.type != TriggerType::Schedule) continue;
            if (!cron_matches(trigger.schedule, now)) continue;
            if (trigger.last_fired_at != std::chrono::system_clock::time_point{} &&
                std::chrono::duration_cast<std::chrono::minutes>(now - trigger.last_fired_at)
                    .count() < 1) {
                continue;
            }
            mgr.mark_trigger_fired(pipeline.name, trigger.name);
            run_callback_(pipeline.name, trigger.name);
        }
    }
}

void PipelineScheduler::dispatch_events() {
    if (!run_callback_) return;
    auto& mgr = PipelineManager::instance();
    for (const auto& ref : PipelineEventBus::instance().take_pending()) {
        const auto status = mgr.pipeline_status(ref.pipeline);
        if (!status || *status == PipelineStatus::Paused || *status == PipelineStatus::Running) {
            continue;
        }
        // The trigger may have been dropped between the event and now.
        if (!mgr.find_trigger(ref.pipeline, ref.trigger)) {
            continue;
        }
        mgr.mark_trigger_fired(ref.pipeline, ref.trigger);
        run_callback_(ref.pipeline, ref.trigger);
    }
}

void PipelineScheduler::wake() {
    {
        std::lock_guard lock{wake_mutex_};
        wake_requested_ = true;
    }
    wake_cv_.notify_one();
}

void PipelineScheduler::start(std::chrono::seconds tick_interval) {
    if (running_.exchange(true)) {
        return;
    }
    tick_interval_ = tick_interval;
    PipelineEventBus::instance().set_wake_callback([this]() { wake(); });
    tick_thread_ = std::thread([this]() {
        std::unique_lock lock{wake_mutex_};
        auto next_tick = std::chrono::steady_clock::now();
        while (running_) {
            lock.unlock();
            // Cron is checked once per interval; events are handled on every wake-up.
            if (std::chrono::steady_clock::now() >= next_tick) {
                tick();
                next_tick = std::chrono::steady_clock::now() + tick_interval_;
            }
            dispatch_events();
            lock.lock();
            wake_cv_.wait_until(lock, next_tick, [this]() { return !running_ || wake_requested_; });
            wake_requested_ = false;
        }
    });
}

void PipelineScheduler::stop() {
    {
        std::lock_guard lock{wake_mutex_};
        running_ = false;
    }
    wake_cv_.notify_all();
    if (tick_thread_.joinable()) {
        tick_thread_.join();
    }
    PipelineEventBus::instance().set_wake_callback(nullptr);
}

auto PipelineScheduler::is_running() const -> bool {
    return running_;
}

} // namespace mnemo::pipelines
