// src/Pipelines/pipeline_scheduler.cpp

#include "Pipelines/pipeline_scheduler.h"
#include "Pipelines/cron_matcher.h"
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

void PipelineScheduler::start(std::chrono::seconds tick_interval) {
    if (running_.exchange(true)) {
        return;
    }
    tick_interval_ = tick_interval;
    tick_thread_ = std::thread([this]() {
        while (running_) {
            tick();
            std::this_thread::sleep_for(tick_interval_);
        }
    });
}

void PipelineScheduler::stop() {
    running_ = false;
    if (tick_thread_.joinable()) {
        tick_thread_.join();
    }
}

auto PipelineScheduler::is_running() const -> bool {
    return running_;
}

} // namespace mnemo::pipelines
