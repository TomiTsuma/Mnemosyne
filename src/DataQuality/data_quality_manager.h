// src/DataQuality/data_quality_manager.h — registry of DATA_QUALITY_RULE entities

#pragma once

#include "DataQuality/data_quality_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::data_quality
{

class DataQualityManager
{
public:
    static auto instance() -> DataQualityManager &;

    /// Validates and stores the rule. With if_not_exists an existing rule is left untouched.
    void create_rule(RuleEntry entry, bool if_not_exists);
    void drop_rule(std::string_view name, bool if_exists);

    [[nodiscard]] auto has_rule(std::string_view name) const -> bool;
    /// Returns a copy so callers never hold a pointer into the registry.
    [[nodiscard]] auto get_rule(std::string_view name) const -> std::optional<RuleEntry>;
    /// Sorted by name.
    [[nodiscard]] auto list_rules() const -> std::vector<RuleEntry>;

    void record_result(std::string_view name, RuleStatus status, uint64_t violations, std::string message);

private:
    DataQualityManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, RuleEntry> rules_;
};

}  // namespace mnemo::data_quality
