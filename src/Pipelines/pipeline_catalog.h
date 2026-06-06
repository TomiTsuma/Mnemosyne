// src/Pipelines/pipeline_catalog.h — Pipeline layer catalog types

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::pipelines {

enum class PipelineStatus {
    Creating,
    Active,
    Running,
    Paused,
    Failed,
    Archived
};

enum class TaskType { Sql, BuiltIn };

enum class TaskStatus { Pending, Running, Succeeded, Failed, Skipped };

enum class TriggerType { Manual, Schedule };

enum class RunStatus { Pending, Running, Succeeded, Failed };

[[nodiscard]] auto pipeline_status_name(PipelineStatus status) -> std::string;
[[nodiscard]] auto task_type_name(TaskType type) -> std::string;
[[nodiscard]] auto task_status_name(TaskStatus status) -> std::string;
[[nodiscard]] auto trigger_type_name(TriggerType type) -> std::string;
[[nodiscard]] auto run_status_name(RunStatus status) -> std::string;

[[nodiscard]] auto parse_task_type(std::string_view name) -> TaskType;
[[nodiscard]] auto parse_trigger_type(std::string_view name) -> TriggerType;

struct TaskEntry {
    std::string name;
    std::string stage_name;
    TaskType type = TaskType::BuiltIn;
    std::string body;
    std::vector<std::string> depends_on;
    uint32_t max_retries = 0;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

struct StageEntry {
    std::string name;
    uint32_t order = 0;
    std::vector<TaskEntry> tasks;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

struct TriggerEntry {
    std::string name;
    TriggerType type = TriggerType::Manual;
    std::string schedule;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    std::chrono::system_clock::time_point last_fired_at{};
};

struct TaskRunRecord {
    std::string task_name;
    std::string stage_name;
    TaskStatus status = TaskStatus::Pending;
    std::string error;
    double duration_ms = 0.0;
    std::chrono::system_clock::time_point started_at{};
    std::chrono::system_clock::time_point ended_at{};
};

struct PipelineRunEntry {
    std::string run_id;
    std::string pipeline_name;
    std::string trigger_name;
    RunStatus status = RunStatus::Pending;
    std::chrono::system_clock::time_point started_at{};
    std::chrono::system_clock::time_point ended_at{};
    double duration_ms = 0.0;
    std::vector<TaskRunRecord> task_runs;
};

struct PipelineEntry {
    std::string name;
    PipelineStatus status = PipelineStatus::Creating;
    std::string owner;
    std::vector<StageEntry> stages;
    std::vector<TriggerEntry> triggers;
    std::vector<PipelineRunEntry> runs;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

void validate_pipeline_entry(const PipelineEntry& entry);
void validate_stage_entry(const StageEntry& entry);
void validate_task_entry(const TaskEntry& entry);
void validate_trigger_entry(const TriggerEntry& entry);

} // namespace mnemo::pipelines
