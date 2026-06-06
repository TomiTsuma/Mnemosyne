// src/Pipelines/pipeline_catalog.cpp

#include "Pipelines/pipeline_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::pipelines {

namespace {

auto iequals(std::string_view a, std::string_view b) -> bool {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

auto pipeline_status_name(PipelineStatus status) -> std::string {
    switch (status) {
        case PipelineStatus::Creating: return "CREATING";
        case PipelineStatus::Active: return "ACTIVE";
        case PipelineStatus::Running: return "RUNNING";
        case PipelineStatus::Paused: return "PAUSED";
        case PipelineStatus::Failed: return "FAILED";
        case PipelineStatus::Archived: return "ARCHIVED";
    }
    return "UNKNOWN";
}

auto task_type_name(TaskType type) -> std::string {
    switch (type) {
        case TaskType::Sql: return "SQL";
        case TaskType::BuiltIn: return "BUILT_IN";
    }
    return "UNKNOWN";
}

auto task_status_name(TaskStatus status) -> std::string {
    switch (status) {
        case TaskStatus::Pending: return "PENDING";
        case TaskStatus::Running: return "RUNNING";
        case TaskStatus::Succeeded: return "SUCCEEDED";
        case TaskStatus::Failed: return "FAILED";
        case TaskStatus::Skipped: return "SKIPPED";
    }
    return "UNKNOWN";
}

auto trigger_type_name(TriggerType type) -> std::string {
    switch (type) {
        case TriggerType::Manual: return "MANUAL";
        case TriggerType::Schedule: return "SCHEDULE";
    }
    return "UNKNOWN";
}

auto run_status_name(RunStatus status) -> std::string {
    switch (status) {
        case RunStatus::Pending: return "PENDING";
        case RunStatus::Running: return "RUNNING";
        case RunStatus::Succeeded: return "SUCCEEDED";
        case RunStatus::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

auto parse_task_type(std::string_view name) -> TaskType {
    if (iequals(name, "SQL")) return TaskType::Sql;
    if (iequals(name, "BUILT_IN") || iequals(name, "BUILTIN")) return TaskType::BuiltIn;
    throw common::Exception{
        "Unknown task type: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_trigger_type(std::string_view name) -> TriggerType {
    if (iequals(name, "MANUAL")) return TriggerType::Manual;
    if (iequals(name, "SCHEDULE")) return TriggerType::Schedule;
    throw common::Exception{
        "Unknown trigger type: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

void validate_pipeline_entry(const PipelineEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Pipeline name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

void validate_stage_entry(const StageEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Stage name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

void validate_task_entry(const TaskEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Task name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.type == TaskType::Sql && entry.body.empty()) {
        throw common::Exception{
            "SQL task body cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

void validate_trigger_entry(const TriggerEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Trigger name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.type == TriggerType::Schedule && entry.schedule.empty()) {
        throw common::Exception{
            "Schedule trigger requires a cron schedule",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

} // namespace mnemo::pipelines
