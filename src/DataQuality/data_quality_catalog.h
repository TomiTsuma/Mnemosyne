// src/DataQuality/data_quality_catalog.h — DATA_QUALITY_RULE catalog types

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mnemo::data_quality
{

enum class RuleKind
{
    Unique,
    Range,
    Expression,
    MinRows
};

enum class RuleAction
{
    Warn,
    Fail
};

enum class RuleStatus
{
    NotRun,
    Passed,
    Warned,
    Failed
};

[[nodiscard]] auto rule_kind_name(RuleKind kind) -> std::string;
[[nodiscard]] auto rule_action_name(RuleAction action) -> std::string;
[[nodiscard]] auto rule_status_name(RuleStatus status) -> std::string;

/// Case-insensitive; throws on an unknown name. QUARANTINE is recognised so the error can say
/// that it is not available yet instead of calling it unknown.
[[nodiscard]] auto parse_rule_kind(std::string_view name) -> RuleKind;
[[nodiscard]] auto parse_rule_action(std::string_view name) -> RuleAction;

/// True for [A-Za-z_][A-Za-z0-9_]*, optionally with one "database." prefix when allow_qualified.
/// Rule targets are spliced into generated SQL, so only such names are ever accepted.
[[nodiscard]] auto is_plain_identifier(std::string_view name, bool allow_qualified) -> bool;

struct RuleEntry
{
    std::string name;
    std::string database;  // database that was current when the rule was created
    std::string table;     // may itself be qualified ("db.table")
    RuleKind kind = RuleKind::MinRows;
    std::string column;
    std::string expression;
    std::optional<double> min_value;
    std::optional<double> max_value;
    uint64_t min_rows = 0;
    RuleAction action = RuleAction::Fail;
    std::chrono::system_clock::time_point created_at{};

    RuleStatus last_status = RuleStatus::NotRun;
    uint64_t last_violations = 0;
    std::string last_message;
    std::chrono::system_clock::time_point last_checked_at{};
};

/// Checks everything that can be known without touching the table.
void validate_rule_entry(const RuleEntry & entry);

}  // namespace mnemo::data_quality
