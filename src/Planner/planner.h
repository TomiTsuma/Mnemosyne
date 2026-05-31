// src/Planner/planner.h — Query planner: transforms analyzed AST into execution plan
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "execution_plan.h"
#include "query_tree.h"
#include "Interpreters/context.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace mnesso::planner {

// ── Planner — takes analyzed query tree and produces execution plan ──
class Planner {
public:
    explicit Planner(interpreters::Context& context);

    // Main entry — plan a query tree
    auto plan(std::shared_ptr<analyzer::IQueryTreeNode> tree)
        -> std::shared_ptr<ExecutionPlan>;

    // ── Planning strategies ──
    std::shared_ptr<ExecutionPlan> plan_select(analyzer::SelectNode& node);
    std::shared_ptr<ExecutionPlan> plan_join(analyzer::JoinNode& node);
    std::shared_ptr<ExecutionPlan> plan_aggregate(analyzer::AggregateNode& node);
    std::shared_ptr<ExecutionPlan> plan_filter(analyzer::FilterNode& node);
    std::shared_ptr<ExecutionPlan> plan_sort(analyzer::SortNode& node);
    std::shared_ptr<ExecutionPlan> plan_limit(analyzer::LimitNode& node);
    std::shared_ptr<ExecutionPlan> plan_table(analyzer::TableNode& node);

    // Cost estimation
    auto estimate_cost(std::shared_ptr<ExecutionPlan> plan) -> double;

    // ── Join ordering heuristics ──
    std::shared_ptr<analyzer::JoinNode> optimize_join_order(
        std::shared_ptr<analyzer::JoinNode> node);

    // ── Predicate pushdown ──
    void push_down_predicates(std::shared_ptr<ExecutionPlan>& plan);

    // ── Index hint usage ──
    std::optional<size_t> find_best_index(
        analyzer::TableNode& node,
        std::shared_ptr<analyzer::FilterNode> filter);

private:
    interpreters::Context& context_;
    std::uint64_t plan_id_ = 0;
};

} // namespace mnesso::planner
