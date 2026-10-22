// src/Pipelines/pipeline_event_bus.cpp

#include "Pipelines/pipeline_event_bus.h"

namespace mnemo::pipelines {

namespace {

// Per thread: pipeline runs are serialised by the execution lock, and the events a run raises are
// raised on the thread that executes it.
thread_local int run_depth = 0;

} // namespace

auto PipelineEventBus::instance() -> PipelineEventBus& {
    static PipelineEventBus bus;
    return bus;
}

PipelineEventBus::RunScope::RunScope() {
    ++run_depth;
}

PipelineEventBus::RunScope::~RunScope() {
    --run_depth;
}

void PipelineEventBus::queue(const std::vector<PipelineManager::TriggerRef>& triggers) {
    if (run_depth > 0 || triggers.empty()) {
        return;
    }
    std::function<void()> wake;
    {
        std::lock_guard lock{mutex_};
        pending_.insert(triggers.begin(), triggers.end());
        wake = wake_;
    }
    if (wake) {
        wake();
    }
}

void PipelineEventBus::table_inserted(std::string_view qualified_table) {
    if (run_depth > 0) {
        return;
    }
    queue(PipelineManager::instance().triggers_watching(TriggerType::TableInsert, qualified_table));
}

void PipelineEventBus::stream_appended(std::string_view stream) {
    if (run_depth > 0) {
        return;
    }
    queue(PipelineManager::instance().triggers_watching(TriggerType::StreamEvent, stream));
}

void PipelineEventBus::webhook_received(const PipelineManager::TriggerRef& trigger) {
    queue({trigger});
}

auto PipelineEventBus::take_pending() -> std::vector<PipelineManager::TriggerRef> {
    std::lock_guard lock{mutex_};
    std::vector<PipelineManager::TriggerRef> out(pending_.begin(), pending_.end());
    pending_.clear();
    return out;
}

void PipelineEventBus::set_wake_callback(std::function<void()> callback) {
    std::lock_guard lock{mutex_};
    wake_ = std::move(callback);
}

} // namespace mnemo::pipelines
