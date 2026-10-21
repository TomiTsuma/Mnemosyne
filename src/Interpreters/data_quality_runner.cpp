// src/Interpreters/data_quality_runner.cpp

#include "Interpreters/data_quality_runner.h"
#include "Interpreters/interpreter_select_query.h"
#include "Columns/column_string.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include "DataQuality/data_quality_manager.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include <algorithm>
#include <charconv>
#include <cctype>

namespace mnemo::interpreters
{

namespace
{

using data_quality::RuleAction;
using data_quality::RuleEntry;
using data_quality::RuleKind;
using data_quality::RuleStatus;

[[noreturn]] void fail(const std::string & message, common::ErrorCode code = common::ErrorCode::LOGICAL_ERROR)
{
    throw common::Exception{message, static_cast<int>(code)};
}

auto parse_select(const std::string & sql) -> std::unique_ptr<parsers::QueryAST>
{
    parsers::Lexer lexer{sql};
    parsers::QueryParser parser{std::move(lexer)};
    auto ast = parser.parse();
    if (!ast || ast->query_type != parsers::QueryAST::QueryType::SELECT)
    {
        fail("Data quality check did not produce a SELECT: " + sql, common::ErrorCode::SYNTAX_ERROR);
    }
    return ast;
}

auto run_count(Context & context, const std::string & sql) -> uint64_t
{
    const auto ast = parse_select(sql);
    const auto block = InterpreterSelectQuery::execute(context, *ast);
    if (block.row_count() == 0 || block.column_count() == 0)
    {
        return 0;
    }
    return static_cast<uint64_t>(block.get_row_value(0, 0).as_int64().value_or(0));
}

/// Sum of the first column over every row; used for the per-group duplicate counts.
auto run_sum(Context & context, const std::string & sql) -> uint64_t
{
    const auto ast = parse_select(sql);
    const auto block = InterpreterSelectQuery::execute(context, *ast);
    uint64_t total = 0;
    for (size_t row = 0; row < block.row_count(); ++row)
    {
        total += static_cast<uint64_t>(block.get_row_value(0, row).as_int64().value_or(0));
    }
    return total;
}

/// Fixed notation, so a bound such as 1e20 is still a literal the lexer understands.
auto format_number(double value) -> std::string
{
    char buffer[400];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
    return std::string(buffer, result.ptr);
}

auto qualified_table(const RuleEntry & rule) -> std::string
{
    if (rule.table.find('.') != std::string::npos || rule.database.empty())
    {
        return rule.table;
    }
    return rule.database + "." + rule.table;
}

auto range_text(const RuleEntry & rule) -> std::string
{
    std::string text;
    if (rule.min_value)
    {
        text += "MIN " + format_number(*rule.min_value);
    }
    if (rule.max_value)
    {
        text += (text.empty() ? "" : " ") + std::string{"MAX "} + format_number(*rule.max_value);
    }
    return text;
}

auto plural(uint64_t count, const char * singular, const char * many) -> std::string
{
    return std::to_string(count) + " " + (count == 1 ? singular : many);
}

struct Outcome
{
    uint64_t violations = 0;
    std::string message;
};

auto check_unique(Context & context, const RuleEntry & rule, const std::string & table) -> Outcome
{
    const auto rows = run_sum(
        context, "SELECT COUNT(*) AS dup FROM " + table + " GROUP BY " + rule.column + " HAVING COUNT(*) > 1");
    if (rows == 0)
    {
        return {0, "every value of " + rule.column + " is unique"};
    }
    return {rows, plural(rows, "row shares", "rows share") + " a " + rule.column + " value with another row"};
}

auto check_filtered(Context & context, const RuleEntry & rule, const std::string & table, const std::string & condition,
                    const std::string & violation_text) -> Outcome
{
    const auto total = run_count(context, "SELECT COUNT(*) AS n FROM " + table);
    const auto good = run_count(context, "SELECT COUNT(*) AS n FROM " + table + " WHERE " + condition);
    const auto bad = total >= good ? total - good : 0;
    if (bad == 0)
    {
        return {0, "all " + plural(total, "row", "rows") + " satisfy " + violation_text};
    }
    return {bad, std::to_string(bad) + " of " + plural(total, "row", "rows") + " violate " + violation_text};
}

auto check_range(Context & context, const RuleEntry & rule, const std::string & table) -> Outcome
{
    std::string condition;
    if (rule.min_value)
    {
        condition += rule.column + " >= " + format_number(*rule.min_value);
    }
    if (rule.max_value)
    {
        condition += (condition.empty() ? "" : " AND ") + rule.column + " <= " + format_number(*rule.max_value);
    }
    return check_filtered(context, rule, table, condition, rule.column + " " + range_text(rule));
}

auto check_min_rows(Context & context, const RuleEntry & rule, const std::string & table) -> Outcome
{
    const auto total = run_count(context, "SELECT COUNT(*) AS n FROM " + table);
    if (total >= rule.min_rows)
    {
        return {0, "table has " + plural(total, "row", "rows")};
    }
    return {rule.min_rows - total, "table has " + plural(total, "row", "rows") + ", needs at least " + std::to_string(rule.min_rows)};
}

auto string_column(const std::vector<RuleEvaluation> & rows, auto member) -> core::ColumnPtr
{
    auto column = std::make_shared<columns::ColumnString>();
    for (const auto & row : rows)
    {
        column->insert(core::Field{std::string{member(row)}});
    }
    return column;
}

auto rule_string_column(const std::vector<RuleEntry> & rules, auto member) -> core::ColumnPtr
{
    auto column = std::make_shared<columns::ColumnString>();
    for (const auto & rule : rules)
    {
        column->insert(core::Field{std::string{member(rule)}});
    }
    return column;
}

auto bound_text(const std::optional<double> & value) -> std::string
{
    return value ? format_number(*value) : std::string{};
}

/// Renders rules as a table shared by SHOW (every rule) and DESCRIBE (one rule).
auto rules_to_block(const std::vector<RuleEntry> & rules) -> core::Block
{
    auto violations = std::make_shared<columns::ColumnVector<int64_t>>();
    for (const auto & rule : rules)
    {
        violations->insert(core::Field{static_cast<int64_t>(rule.last_violations)});
    }

    core::Block block;
    block.add_column("name", rule_string_column(rules, [](const auto & r) { return r.name; }));
    block.add_column("table", rule_string_column(rules, [](const auto & r) { return r.table; }));
    block.add_column("check", rule_string_column(rules, [](const auto & r) { return data_quality::rule_kind_name(r.kind); }));
    block.add_column("column", rule_string_column(rules, [](const auto & r) { return r.column; }));
    block.add_column("expression", rule_string_column(rules, [](const auto & r) { return r.expression; }));
    block.add_column("action", rule_string_column(rules, [](const auto & r) { return data_quality::rule_action_name(r.action); }));
    block.add_column("min", rule_string_column(rules, [](const auto & r) { return bound_text(r.min_value); }));
    block.add_column("max", rule_string_column(rules, [](const auto & r) { return bound_text(r.max_value); }));
    block.add_column("min_rows", rule_string_column(rules, [](const auto & r) { return std::to_string(r.min_rows); }));
    block.add_column("last_status", rule_string_column(rules, [](const auto & r) { return data_quality::rule_status_name(r.last_status); }));
    block.add_column("last_violations", violations);
    block.add_column("last_message", rule_string_column(rules, [](const auto & r) { return r.last_message; }));
    return block;
}

}  // namespace

void DataQualityRunner::validate_expression(const std::string & expression)
{
    const auto parse_where = [&](const std::string & sql)
    {
        const auto ast = parse_select(sql);
        const auto & select = ast->select;
        if (!select.where || !select.group_by.empty() || select.having || select.has_limit || !select.order_by.empty()
            || !select.joins.empty())
        {
            fail("EXPRESSION must be a single condition, not a full query", common::ErrorCode::SYNTAX_ERROR);
        }
    };
    // Both forms must parse: the bare one rejects stray closing parentheses, and the wrapped one
    // (what evaluation uses) rejects trailing clauses such as ORDER BY.
    parse_where("SELECT 1 FROM t WHERE " + expression);
    parse_where("SELECT 1 FROM t WHERE (" + expression + ")");
}

auto DataQualityRunner::evaluate(Context & context, const RuleEntry & rule) -> RuleEvaluation
{
    const auto table = qualified_table(rule);

    Outcome outcome;
    switch (rule.kind)
    {
        case RuleKind::Unique:
            outcome = check_unique(context, rule, table);
            break;
        case RuleKind::Range:
            outcome = check_range(context, rule, table);
            break;
        case RuleKind::Expression:
            outcome = check_filtered(context, rule, table, "(" + rule.expression + ")", rule.expression);
            break;
        case RuleKind::MinRows:
            outcome = check_min_rows(context, rule, table);
            break;
    }

    RuleStatus status = RuleStatus::Passed;
    if (outcome.violations > 0)
    {
        status = rule.action == RuleAction::Warn ? RuleStatus::Warned : RuleStatus::Failed;
    }
    data_quality::DataQualityManager::instance().record_result(rule.name, status, outcome.violations, outcome.message);

    RuleEvaluation evaluation;
    evaluation.rule = rule.name;
    evaluation.table = rule.table;
    evaluation.check = data_quality::rule_kind_name(rule.kind);
    evaluation.action = data_quality::rule_action_name(rule.action);
    evaluation.status = status;
    evaluation.violations = outcome.violations;
    evaluation.message = std::move(outcome.message);
    return evaluation;
}

auto DataQualityRunner::evaluate_named(Context & context, std::string_view name) -> RuleEvaluation
{
    const auto rule = data_quality::DataQualityManager::instance().get_rule(name);
    if (!rule)
    {
        fail("Unknown data quality rule: " + std::string{name}, common::ErrorCode::UNKNOWN_TABLE);
    }
    return evaluate(context, *rule);
}

auto DataQualityRunner::split_rule_names(std::string_view body) -> std::vector<std::string>
{
    std::vector<std::string> names;
    std::string current;
    const auto flush = [&]()
    {
        const auto first = current.find_first_not_of(" \t\r\n");
        if (first != std::string::npos)
        {
            const auto last = current.find_last_not_of(" \t\r\n");
            names.push_back(current.substr(first, last - first + 1));
        }
        current.clear();
    };
    for (const char c : body)
    {
        if (c == ',')
        {
            flush();
        }
        else
        {
            current.push_back(c);
        }
    }
    flush();
    return names;
}

auto DataQualityRunner::execute_run(Context & context, const parsers::QueryAST & query) -> core::Block
{
    const auto evaluation = evaluate_named(context, query.quality_control.rule_name);
    const std::vector<RuleEvaluation> rows{evaluation};

    auto violations = std::make_shared<columns::ColumnVector<int64_t>>();
    violations->insert(core::Field{static_cast<int64_t>(evaluation.violations)});

    core::Block block;
    block.add_column("rule", string_column(rows, [](const auto & r) { return r.rule; }));
    block.add_column("table", string_column(rows, [](const auto & r) { return r.table; }));
    block.add_column("check", string_column(rows, [](const auto & r) { return r.check; }));
    block.add_column("action", string_column(rows, [](const auto & r) { return r.action; }));
    block.add_column("status", string_column(rows, [](const auto & r) { return data_quality::rule_status_name(r.status); }));
    block.add_column("violations", violations);
    block.add_column("message", string_column(rows, [](const auto & r) { return r.message; }));
    return block;
}

auto DataQualityRunner::execute_show() -> core::Block
{
    return rules_to_block(data_quality::DataQualityManager::instance().list_rules());
}

auto DataQualityRunner::execute_describe(std::string_view name) -> core::Block
{
    const auto rule = data_quality::DataQualityManager::instance().get_rule(name);
    if (!rule)
    {
        fail("Unknown data quality rule: " + std::string{name}, common::ErrorCode::UNKNOWN_TABLE);
    }
    return rules_to_block({*rule});
}

}  // namespace mnemo::interpreters
