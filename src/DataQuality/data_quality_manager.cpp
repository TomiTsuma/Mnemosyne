// src/DataQuality/data_quality_manager.cpp

#include "DataQuality/data_quality_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::data_quality
{

namespace
{

[[noreturn]] void unknown_rule(std::string_view name)
{
    throw common::Exception{
        "Unknown data quality rule: " + std::string{name}, static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
}

}  // namespace

auto DataQualityManager::instance() -> DataQualityManager &
{
    static DataQualityManager manager;
    return manager;
}

void DataQualityManager::create_rule(RuleEntry entry, bool if_not_exists)
{
    validate_rule_entry(entry);

    std::lock_guard lock(mutex_);
    if (rules_.contains(entry.name))
    {
        if (if_not_exists)
        {
            return;
        }
        throw common::Exception{
            "Data quality rule already exists: " + entry.name, static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    entry.created_at = std::chrono::system_clock::now();
    entry.last_status = RuleStatus::NotRun;
    rules_.emplace(entry.name, std::move(entry));
}

void DataQualityManager::drop_rule(std::string_view name, bool if_exists)
{
    std::lock_guard lock(mutex_);
    const auto it = rules_.find(std::string{name});
    if (it == rules_.end())
    {
        if (if_exists)
        {
            return;
        }
        unknown_rule(name);
    }
    rules_.erase(it);
}

auto DataQualityManager::has_rule(std::string_view name) const -> bool
{
    std::lock_guard lock(mutex_);
    return rules_.contains(std::string{name});
}

auto DataQualityManager::get_rule(std::string_view name) const -> std::optional<RuleEntry>
{
    std::lock_guard lock(mutex_);
    const auto it = rules_.find(std::string{name});
    if (it == rules_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

auto DataQualityManager::list_rules() const -> std::vector<RuleEntry>
{
    std::vector<RuleEntry> out;
    {
        std::lock_guard lock(mutex_);
        out.reserve(rules_.size());
        for (const auto & [name, entry] : rules_)
        {
            out.push_back(entry);
        }
    }
    std::sort(out.begin(), out.end(), [](const RuleEntry & a, const RuleEntry & b) { return a.name < b.name; });
    return out;
}

void DataQualityManager::record_result(std::string_view name, RuleStatus status, uint64_t violations, std::string message)
{
    std::lock_guard lock(mutex_);
    const auto it = rules_.find(std::string{name});
    if (it == rules_.end())
    {
        unknown_rule(name);
    }
    it->second.last_status = status;
    it->second.last_violations = violations;
    it->second.last_message = std::move(message);
    it->second.last_checked_at = std::chrono::system_clock::now();
}

}  // namespace mnemo::data_quality
