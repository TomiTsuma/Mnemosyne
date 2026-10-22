// src/Pipelines/pipeline_event_bus.h — queues pipeline triggers that fired because of an event

#pragma once

#include "Pipelines/pipeline_manager.h"
#include <functional>
#include <mutex>
#include <set>
#include <string_view>
#include <vector>

namespace mnemo::pipelines {

/// Turns events (rows inserted into a table, an event appended to a stream, a webhook call) into
/// queued triggers, and hands them to the PipelineScheduler, which runs the pipelines.
///
/// Queued triggers are a set: a burst of events before the scheduler gets to them yields one run,
/// not one run per event.
///
/// Events raised while a pipeline run is executing are ignored (see RunScope), so a pipeline whose
/// tasks write the table it watches cannot trigger itself, and two pipelines cannot trigger each
/// other in a loop. The cost is that one pipeline's output does not start another through a
/// trigger.
class PipelineEventBus {
public:
    static auto instance() -> PipelineEventBus&;

    /// While one of these is alive on a thread, events raised on that thread are ignored.
    /// PipelineExecutor holds one for the length of a run.
    class RunScope {
    public:
        RunScope();
        ~RunScope();
        RunScope(const RunScope&) = delete;
        RunScope& operator=(const RunScope&) = delete;
    };

    /// `qualified_table` is "database.table".
    void table_inserted(std::string_view qualified_table);
    void stream_appended(std::string_view stream);
    /// Queues one trigger directly; the HTTP route has already checked it is a webhook trigger.
    void webhook_received(const PipelineManager::TriggerRef& trigger);

    /// Everything queued since the last call, each trigger once.
    [[nodiscard]] auto take_pending() -> std::vector<PipelineManager::TriggerRef>;

    /// Called whenever something new is queued, outside the bus's lock.
    void set_wake_callback(std::function<void()> callback);

private:
    PipelineEventBus() = default;

    void queue(const std::vector<PipelineManager::TriggerRef>& triggers);

    std::mutex mutex_;
    std::set<PipelineManager::TriggerRef> pending_;
    std::function<void()> wake_;
};

} // namespace mnemo::pipelines
