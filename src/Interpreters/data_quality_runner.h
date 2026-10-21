// src/Interpreters/data_quality_runner.h — evaluates DATA_QUALITY_RULE entities

#pragma once

#include "Core/block.h"
#include "DataQuality/data_quality_catalog.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::interpreters
{

struct RuleEvaluation
{
    std::string rule;
    std::string table;
    std::string check;
    std::string action;
    data_quality::RuleStatus status = data_quality::RuleStatus::NotRun;
    uint64_t violations = 0;
    std::string message;
};

class DataQualityRunner
{
public:
    /// Runs the rule against the current data and stores the outcome on the rule.
    /// A violation is a result (WARNED or FAILED), not an exception. Anything that stops the
    /// check from running at all (unknown table or column, bad expression) is thrown, so a rule
    /// that could not be evaluated is never reported as passing.
    static auto evaluate(Context & context, const data_quality::RuleEntry & rule) -> RuleEvaluation;
    static auto evaluate_named(Context & context, std::string_view name) -> RuleEvaluation;

    /// Splits the body of a QUALITY task ("a, b,c") into rule names, dropping empty items.
    static auto split_rule_names(std::string_view body) -> std::vector<std::string>;

    /// RUN DATA_QUALITY_RULE <name>
    static auto execute_run(Context & context, const parsers::QueryAST & query) -> core::Block;

    /// SHOW DATA_QUALITY_RULES — one row per registered rule, sorted by name.
    static auto execute_show() -> core::Block;

    /// DESCRIBE DATA_QUALITY_RULE <name> — a single-row description, or an error if it is unknown.
    static auto execute_describe(std::string_view name) -> core::Block;

    /// Checks that an EXPRESSION condition is a complete, standalone boolean expression, so it can
    /// be safely wrapped in parentheses when the rule is evaluated.
    static void validate_expression(const std::string & expression);
};

}  // namespace mnemo::interpreters
