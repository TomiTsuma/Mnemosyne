// src/Pipelines/pipeline_manager.cpp

#include "Pipelines/pipeline_manager.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace mnemo::pipelines {

auto PipelineManager::instance() -> PipelineManager& {
    static PipelineManager inst;
    return inst;
}

auto PipelineManager::make_run_id() -> std::string {
    return "run_" + std::to_string(++run_counter_);
}

auto PipelineManager::require_pipeline(std::string_view name) -> PipelineEntry& {
    auto it = pipelines_.find(std::string{name});
    if (it == pipelines_.end()) {
        throw common::Exception{
            "Unknown pipeline: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto PipelineManager::require_pipeline(std::string_view name) const -> const PipelineEntry& {
    auto it = pipelines_.find(std::string{name});
    if (it == pipelines_.end()) {
        throw common::Exception{
            "Unknown pipeline: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto PipelineManager::detect_cycle_unlocked(std::string_view pipeline_name) const -> bool {
    const auto& pipeline = require_pipeline(pipeline_name);
    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const auto& stage : pipeline.stages) {
        for (const auto& task : stage.tasks) {
            graph[task.name] = task.depends_on;
        }
    }
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        if (visiting.contains(node)) return true;
        if (visited.contains(node)) return false;
        visiting.insert(node);
        for (const auto& dep : graph[node]) {
            if (dfs(dep)) return true;
        }
        visiting.erase(node);
        visited.insert(node);
        return false;
    };
    for (const auto& [name, _] : graph) {
        if (dfs(name)) return true;
    }
    return false;
}

auto PipelineManager::create_pipeline(PipelineEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    validate_pipeline_entry(entry);
    if (pipelines_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Pipeline already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    entry.status = PipelineStatus::Active;
    pipelines_.emplace(entry.name, std::move(entry));
}

auto PipelineManager::drop_pipeline(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = pipelines_.find(name);
    if (it == pipelines_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown pipeline: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    pipelines_.erase(it);
}

auto PipelineManager::alter_pipeline(std::string_view name,
                                     std::optional<std::string> owner) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(name);
    if (owner) {
        pipeline.owner = *owner;
    }
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::pause_pipeline(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(name);
    if (pipeline.status == PipelineStatus::Running) {
        throw common::Exception{
            "Cannot pause running pipeline: " + std::string{name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    pipeline.status = PipelineStatus::Paused;
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::resume_pipeline(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(name);
    if (pipeline.status != PipelineStatus::Paused) {
        throw common::Exception{
            "Pipeline is not paused: " + std::string{name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    pipeline.status = PipelineStatus::Active;
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::find_stage(PipelineEntry& pipeline, std::string_view stage_name)
    -> StageEntry* {
    for (auto& stage : pipeline.stages) {
        if (stage.name == stage_name) return &stage;
    }
    return nullptr;
}

auto PipelineManager::create_stage(std::string_view pipeline_name, StageEntry stage,
                                   bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    validate_stage_entry(stage);
    if (auto* existing = find_stage(pipeline, stage.name)) {
        (void)existing;
        if (if_not_exists) return;
        throw common::Exception{
            "Stage already exists: " + stage.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto now = std::chrono::system_clock::now();
    stage.created_at = now;
    stage.updated_at = now;
    pipeline.stages.push_back(std::move(stage));
    std::sort(pipeline.stages.begin(), pipeline.stages.end(),
              [](const StageEntry& a, const StageEntry& b) { return a.order < b.order; });
    pipeline.updated_at = now;
}

auto PipelineManager::drop_stage(std::string_view pipeline_name, std::string stage_name,
                                 bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    auto it = std::remove_if(pipeline.stages.begin(), pipeline.stages.end(),
                             [&](const StageEntry& s) { return s.name == stage_name; });
    if (it == pipeline.stages.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown stage: " + stage_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    pipeline.stages.erase(it, pipeline.stages.end());
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::create_task(std::string_view pipeline_name, std::string_view stage_name,
                                  TaskEntry task, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    auto* stage = find_stage(pipeline, stage_name);
    if (!stage) {
        throw common::Exception{
            "Unknown stage: " + std::string{stage_name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    validate_task_entry(task);
    task.stage_name = stage->name;
    for (const auto& existing : stage->tasks) {
        if (existing.name == task.name) {
            if (if_not_exists) return;
            throw common::Exception{
                "Task already exists: " + task.name,
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
    }
    const auto now = std::chrono::system_clock::now();
    task.created_at = now;
    task.updated_at = now;
    stage->tasks.push_back(std::move(task));
    pipeline.updated_at = now;
}

auto PipelineManager::drop_task(std::string_view pipeline_name, std::string_view stage_name,
                                std::string task_name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    auto* stage = find_stage(pipeline, stage_name);
    if (!stage) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown stage: " + std::string{stage_name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    auto it = std::remove_if(stage->tasks.begin(), stage->tasks.end(),
                             [&](const TaskEntry& t) { return t.name == task_name; });
    if (it == stage->tasks.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown task: " + task_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    stage->tasks.erase(it, stage->tasks.end());
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::create_trigger(std::string_view pipeline_name, TriggerEntry trigger,
                                     bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    validate_trigger_entry(trigger);
    for (const auto& existing : pipeline.triggers) {
        if (existing.name == trigger.name) {
            if (if_not_exists) return;
            throw common::Exception{
                "Trigger already exists: " + trigger.name,
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
    }
    const auto now = std::chrono::system_clock::now();
    trigger.created_at = now;
    trigger.updated_at = now;
    pipeline.triggers.push_back(std::move(trigger));
    pipeline.updated_at = now;
}

auto PipelineManager::drop_trigger(std::string_view pipeline_name, std::string trigger_name,
                                   bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    auto it = std::remove_if(pipeline.triggers.begin(), pipeline.triggers.end(),
                             [&](const TriggerEntry& t) { return t.name == trigger_name; });
    if (it == pipeline.triggers.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown trigger: " + trigger_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    pipeline.triggers.erase(it, pipeline.triggers.end());
    pipeline.updated_at = std::chrono::system_clock::now();
}

auto PipelineManager::get_pipeline(std::string_view name) -> PipelineEntry* {
    std::lock_guard lock{mutex_};
    auto it = pipelines_.find(std::string{name});
    if (it == pipelines_.end()) return nullptr;
    return &it->second;
}

auto PipelineManager::get_pipeline(std::string_view name) const -> const PipelineEntry* {
    std::lock_guard lock{mutex_};
    auto it = pipelines_.find(std::string{name});
    if (it == pipelines_.end()) return nullptr;
    return &it->second;
}

auto PipelineManager::has_pipeline(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return pipelines_.contains(std::string{name});
}

auto PipelineManager::list_pipelines() const -> std::vector<PipelineEntry> {
    std::lock_guard lock{mutex_};
    std::vector<PipelineEntry> result;
    result.reserve(pipelines_.size());
    for (const auto& [_, entry] : pipelines_) {
        result.push_back(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const PipelineEntry& a, const PipelineEntry& b) { return a.name < b.name; });
    return result;
}

auto PipelineManager::begin_run(std::string_view pipeline_name,
                                std::string_view trigger_name) -> PipelineRunEntry& {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    if (pipeline.status == PipelineStatus::Paused) {
        throw common::Exception{
            "Pipeline is paused: " + std::string{pipeline_name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    if (detect_cycle_unlocked(pipeline_name)) {
        throw common::Exception{
            "Pipeline has cyclic task dependencies: " + std::string{pipeline_name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    PipelineRunEntry run;
    run.run_id = make_run_id();
    run.pipeline_name = std::string{pipeline_name};
    run.trigger_name = std::string{trigger_name};
    run.status = RunStatus::Running;
    run.started_at = std::chrono::system_clock::now();
    pipeline.runs.push_back(run);
    pipeline.status = PipelineStatus::Running;
    pipeline.updated_at = run.started_at;
    return pipeline.runs.back();
}

auto PipelineManager::finish_run(std::string_view pipeline_name, std::string_view run_id,
                                 RunStatus status, double duration_ms) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    for (auto& run : pipeline.runs) {
        if (run.run_id == run_id) {
            run.status = status;
            run.ended_at = std::chrono::system_clock::now();
            run.duration_ms = duration_ms;
            pipeline.status = status == RunStatus::Succeeded ? PipelineStatus::Active
                                                             : PipelineStatus::Failed;
            pipeline.updated_at = run.ended_at;
            return;
        }
    }
    throw common::Exception{
        "Unknown pipeline run: " + std::string{run_id},
        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
}

auto PipelineManager::record_task_run(std::string_view pipeline_name, std::string_view run_id,
                                      TaskRunRecord record) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    for (auto& run : pipeline.runs) {
        if (run.run_id == run_id) {
            run.task_runs.push_back(std::move(record));
            return;
        }
    }
}

auto PipelineManager::list_runs(std::string_view pipeline_name) const
    -> std::vector<PipelineRunEntry> {
    std::lock_guard lock{mutex_};
    if (pipeline_name.empty()) {
        std::vector<PipelineRunEntry> all;
        for (const auto& [_, pipeline] : pipelines_) {
            all.insert(all.end(), pipeline.runs.begin(), pipeline.runs.end());
        }
        return all;
    }
    return require_pipeline(pipeline_name).runs;
}

auto PipelineManager::detect_cycle(std::string_view pipeline_name) const -> bool {
    std::lock_guard lock{mutex_};
    return detect_cycle_unlocked(pipeline_name);
}

auto PipelineManager::topological_task_order(std::string_view pipeline_name) const
    -> std::vector<TaskEntry> {
    std::lock_guard lock{mutex_};
    const auto& pipeline = require_pipeline(pipeline_name);
    std::vector<TaskEntry> all_tasks;
    for (const auto& stage : pipeline.stages) {
        for (const auto& task : stage.tasks) {
            all_tasks.push_back(task);
        }
    }
    std::unordered_map<std::string, TaskEntry> by_name;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    std::unordered_map<std::string, int> indegree;
    for (const auto& task : all_tasks) {
        by_name[task.name] = task;
        indegree[task.name] = static_cast<int>(task.depends_on.size());
        for (const auto& dep : task.depends_on) {
            if (!by_name.contains(dep) && dep != task.name) {
                // allow deps defined in other stages - check all tasks
                bool found = false;
                for (const auto& other : all_tasks) {
                    if (other.name == dep) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throw common::Exception{
                        "Unknown dependency: " + dep,
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
            }
            dependents[dep].push_back(task.name);
        }
    }
    for (const auto& task : all_tasks) {
        by_name[task.name] = task;
    }
    std::vector<std::string> queue;
    for (const auto& [name, deg] : indegree) {
        if (deg == 0) queue.push_back(name);
    }
    std::sort(queue.begin(), queue.end());
    std::vector<TaskEntry> ordered;
    while (!queue.empty()) {
        const auto name = queue.front();
        queue.erase(queue.begin());
        ordered.push_back(by_name.at(name));
        for (const auto& dependent : dependents[name]) {
            --indegree[dependent];
            if (indegree[dependent] == 0) {
                queue.push_back(dependent);
                std::sort(queue.begin(), queue.end());
            }
        }
    }
    if (ordered.size() != all_tasks.size()) {
        throw common::Exception{
            "Pipeline has cyclic task dependencies",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return ordered;
}

auto PipelineManager::mark_trigger_fired(std::string_view pipeline_name,
                                         std::string_view trigger_name) -> void {
    std::lock_guard lock{mutex_};
    auto& pipeline = require_pipeline(pipeline_name);
    for (auto& trigger : pipeline.triggers) {
        if (trigger.name == trigger_name) {
            trigger.last_fired_at = std::chrono::system_clock::now();
            trigger.updated_at = trigger.last_fired_at;
            return;
        }
    }
}

} // namespace mnemo::pipelines
