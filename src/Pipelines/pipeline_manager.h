// src/Pipelines/pipeline_manager.h — Cluster-wide pipeline registry

#pragma once

#include "Pipelines/pipeline_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::pipelines {

class PipelineManager {
public:
    static auto instance() -> PipelineManager&;

    auto create_pipeline(PipelineEntry entry, bool if_not_exists) -> void;
    auto drop_pipeline(std::string name, bool if_exists) -> void;
    auto alter_pipeline(std::string_view name, std::optional<std::string> owner) -> void;
    auto pause_pipeline(std::string_view name) -> void;
    auto resume_pipeline(std::string_view name) -> void;

    auto create_stage(std::string_view pipeline_name, StageEntry stage,
                      bool if_not_exists) -> void;
    auto drop_stage(std::string_view pipeline_name, std::string stage_name,
                    bool if_exists) -> void;

    auto create_task(std::string_view pipeline_name, std::string_view stage_name,
                     TaskEntry task, bool if_not_exists) -> void;
    auto drop_task(std::string_view pipeline_name, std::string_view stage_name,
                   std::string task_name, bool if_exists) -> void;

    auto create_trigger(std::string_view pipeline_name, TriggerEntry trigger,
                        bool if_not_exists) -> void;
    auto drop_trigger(std::string_view pipeline_name, std::string trigger_name,
                      bool if_exists) -> void;

    auto get_pipeline(std::string_view name) -> PipelineEntry*;
    auto get_pipeline(std::string_view name) const -> const PipelineEntry*;
    [[nodiscard]] auto has_pipeline(std::string_view name) const -> bool;
    [[nodiscard]] auto list_pipelines() const -> std::vector<PipelineEntry>;

    auto begin_run(std::string_view pipeline_name, std::string_view trigger_name)
        -> PipelineRunEntry&;
    auto finish_run(std::string_view pipeline_name, std::string_view run_id,
                    RunStatus status, double duration_ms) -> void;
    auto record_task_run(std::string_view pipeline_name, std::string_view run_id,
                         TaskRunRecord record) -> void;

    [[nodiscard]] auto list_runs(std::string_view pipeline_name) const
        -> std::vector<PipelineRunEntry>;
    [[nodiscard]] auto detect_cycle(std::string_view pipeline_name) const -> bool;
    [[nodiscard]] auto topological_task_order(std::string_view pipeline_name) const
        -> std::vector<TaskEntry>;

    auto mark_trigger_fired(std::string_view pipeline_name,
                            std::string_view trigger_name) -> void;

private:
    PipelineManager() = default;

    auto require_pipeline(std::string_view name) -> PipelineEntry&;
    auto require_pipeline(std::string_view name) const -> const PipelineEntry&;
    auto find_stage(PipelineEntry& pipeline, std::string_view stage_name) -> StageEntry*;
    auto make_run_id() -> std::string;
    [[nodiscard]] auto detect_cycle_unlocked(std::string_view pipeline_name) const -> bool;

    mutable std::mutex mutex_;
    mutable uint64_t run_counter_ = 0;
    std::unordered_map<std::string, PipelineEntry> pipelines_;
};

} // namespace mnemo::pipelines
