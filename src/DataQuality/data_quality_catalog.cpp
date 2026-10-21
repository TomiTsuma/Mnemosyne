// src/DataQuality/data_quality_catalog.cpp

#include "DataQuality/data_quality_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::data_quality
{

namespace
{

auto upper(std::string_view text) -> std::string
{
    std::string out{text};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

[[noreturn]] void fail(const std::string & message, common::ErrorCode code = common::ErrorCode::SYNTAX_ERROR)
{
    throw common::Exception{message, static_cast<int>(code)};
}

auto is_identifier_part(std::string_view part) -> bool
{
    if (part.empty() || !(std::isalpha(static_cast<unsigned char>(part.front())) || part.front() == '_'))
    {
        return false;
    }
    return std::all_of(part.begin(), part.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}

}  // namespace

auto rule_kind_name(RuleKind kind) -> std::string
{
    switch (kind)
    {
        case RuleKind::Unique: return "UNIQUE";
        case RuleKind::Range: return "RANGE";
        case RuleKind::Expression: return "EXPRESSION";
        case RuleKind::MinRows: return "MIN_ROWS";
    }
    return "UNKNOWN";
}

auto rule_action_name(RuleAction action) -> std::string
{
    switch (action)
    {
        case RuleAction::Warn: return "WARN";
        case RuleAction::Fail: return "FAIL";
    }
    return "UNKNOWN";
}

auto rule_status_name(RuleStatus status) -> std::string
{
    switch (status)
    {
        case RuleStatus::NotRun: return "NOT_RUN";
        case RuleStatus::Passed: return "PASSED";
        case RuleStatus::Warned: return "WARNED";
        case RuleStatus::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

auto parse_rule_kind(std::string_view name) -> RuleKind
{
    const auto text = upper(name);
    if (text == "UNIQUE") return RuleKind::Unique;
    if (text == "RANGE") return RuleKind::Range;
    if (text == "EXPRESSION") return RuleKind::Expression;
    if (text == "MIN_ROWS") return RuleKind::MinRows;
    fail("Unknown data quality check '" + std::string{name} + "'; expected UNIQUE, RANGE, EXPRESSION or MIN_ROWS");
}

auto parse_rule_action(std::string_view name) -> RuleAction
{
    const auto text = upper(name);
    if (text == "WARN") return RuleAction::Warn;
    if (text == "FAIL") return RuleAction::Fail;
    if (text == "QUARANTINE")
    {
        fail("ACTION QUARANTINE is not supported yet; use WARN or FAIL", common::ErrorCode::NOT_IMPLEMENTED);
    }
    fail("Unknown data quality action '" + std::string{name} + "'; expected WARN or FAIL");
}

auto is_plain_identifier(std::string_view name, bool allow_qualified) -> bool
{
    const auto dot = name.find('.');
    if (dot == std::string_view::npos)
    {
        return is_identifier_part(name);
    }
    return allow_qualified && is_identifier_part(name.substr(0, dot)) && is_identifier_part(name.substr(dot + 1));
}

void validate_rule_entry(const RuleEntry & entry)
{
    if (!is_plain_identifier(entry.name, false))
    {
        fail("Invalid data quality rule name '" + entry.name + "'");
    }
    if (!is_plain_identifier(entry.table, true))
    {
        fail("Invalid table name '" + entry.table + "' in data quality rule");
    }

    switch (entry.kind)
    {
        case RuleKind::Unique:
            if (!is_plain_identifier(entry.column, false))
            {
                fail("UNIQUE needs a column name, got '" + entry.column + "'");
            }
            break;
        case RuleKind::Range:
            if (!is_plain_identifier(entry.column, false))
            {
                fail("RANGE needs a column name, got '" + entry.column + "'");
            }
            if (!entry.min_value && !entry.max_value)
            {
                fail("RANGE needs MIN, MAX or both");
            }
            if (entry.min_value && entry.max_value && *entry.min_value > *entry.max_value)
            {
                fail("RANGE MIN must not be greater than MAX");
            }
            break;
        case RuleKind::Expression:
            if (entry.expression.empty())
            {
                fail("EXPRESSION needs a condition, e.g. EXPRESSION 'amount >= 0'");
            }
            break;
        case RuleKind::MinRows:
            if (entry.min_rows == 0)
            {
                fail("MIN_ROWS needs a count of at least 1");
            }
            break;
    }
}

}  // namespace mnemo::data_quality
