// src/Planner/planner.cpp — Query planner for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Planner/planner.h"
#include "Analyzer/query_tree.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::planner {

// ── Planner ──

Planner::Planner(interpreters::Context& context) : context_{context} {}

auto Planner::plan(std::shared_ptr<analyzer::IQueryTreeNode> tree)
    -> std::shared_ptr<ExecutionPlan> {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "plan-" + std::to_string(plan_id_));

    if (!tree) {
        throw common::Exception{
            "Planner::plan: null query tree",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    // Dispatch based on node type
    std::string type = tree->node_type();
    if (type == "Select") {
        auto* select = dynamic_cast<analyzer::SelectNode*>(tree.get());
        if (select) {
            plan->root = plan_select(*select)->root;
        }
    } else if (type == "Table") {
        auto* table = dynamic_cast<analyzer::TableNode*>(tree.get());
        if (table) {
            plan->root = plan_table(*table)->root;
        }
    } else if (type == "Join") {
        auto* join = dynamic_cast<analyzer::JoinNode*>(tree.get());
        if (join) {
            plan->root = plan_join(*join)->root;
        }
    } else if (type == "Aggregate") {
        auto* agg = dynamic_cast<analyzer::AggregateNode*>(tree.get());
        if (agg) {
            plan->root = plan_aggregate(*agg)->root;
        }
    } else if (type == "Filter") {
        auto* filter = dynamic_cast<analyzer::FilterNode*>(tree.get());
        if (filter) {
            plan->root = plan_filter(*filter)->root;
        }
    } else if (type == "Sort") {
        auto* sort = dynamic_cast<analyzer::SortNode*>(tree.get());
        if (sort) {
            plan->root = plan_sort(*sort)->root;
        }
    } else if (type == "Limit") {
        auto* limit = dynamic_cast<analyzer::LimitNode*>(tree.get());
        if (limit) {
            plan->root = plan_limit(*limit)->root;
        }
    } else {
        throw common::Exception{
            "Planner::plan: unknown node type: " + type,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    return plan;
}

// ── Planning strategies ──

std::shared_ptr<ExecutionPlan> Planner::plan_select(analyzer::SelectNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "select-" + std::to_string(plan_id_));

    // Process FROM clause (base table scan)
    std::shared_ptr<PlanNode> scan;
    if (node.from) {
        if (node.node_type() == "Table") {
            auto* tbl = dynamic_cast<analyzer::TableNode*>(node.from.get());
            if (tbl) {
                scan = plan_table(*tbl)->root;
            }
        }
    }

    if (!scan) {
        scan = std::make_shared<PlanNode>(PlanNode::Type::SCAN);
        scan->name = "(unknown table)";
    }

    // Apply WHERE filter
    std::shared_ptr<PlanNode> current = scan;
    if (node.where) {
        auto filter = std::make_shared<PlanNode>(PlanNode::Type::FILTER);
        filter->name = "WHERE";
        filter->child = scan;
        current = filter;
    }

    // Apply GROUP BY
    if (!node.group_by.empty()) {
        auto agg = std::make_shared<PlanNode>(PlanNode::Type::GROUP_BY);
        agg->name = "GROUP BY";
        agg->child = current;
        current = agg;
    }

    // Apply PROJECT (SELECT columns)
    auto project = std::make_shared<PlanNode>(PlanNode::Type::PROJECT);
    project->name = "SELECT";
    project->columns = node.columns.size() > 0
        ? std::vector<std::string>(node.columns.size())
        : std::vector<std::string>{"*"};
    project->child = current;
    plan->root = project;

    // Apply ORDER BY
    if (!node.order_by.empty()) {
        auto sort = std::make_shared<PlanNode>(PlanNode::Type::SORT);
        sort->name = "ORDER BY";
        sort->child = plan->root;
        plan->root = sort;
    }

    // Apply LIMIT
    if (node.limit.first > 0) {
        auto limit = std::make_shared<PlanNode>(PlanNode::Type::LIMIT);
        limit->name = "LIMIT";
        limit->limit = node.limit.first;
        if (node.limit.second.has_value()) {
            limit->offset = node.limit.second.value();
        }
        limit->child = plan->root;
        plan->root = limit;
    }

    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_join(analyzer::JoinNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "join-" + std::to_string(plan_id_));

    auto join = std::make_shared<PlanNode>(PlanNode::Type::SCAN); // Placeholder
    join->name = "JOIN (" + node.join_type + ")";
    join->child = node.left ? std::make_shared<PlanNode>(PlanNode::Type::SCAN) : nullptr;
    if (node.right) {
        join->children.push_back(
            std::make_shared<PlanNode>(PlanNode::Type::SCAN));
    }
    plan->root = join;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_aggregate(analyzer::AggregateNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "agg-" + std::to_string(plan_id_));

    auto agg = std::make_shared<PlanNode>(PlanNode::Type::GROUP_BY);
    agg->name = "AGGREGATE";
    if (node.child) {
        agg->child = plan_select(
            *dynamic_cast<analyzer::SelectNode*>(node.child.get())
        )->root;
    }
    plan->root = agg;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_filter(analyzer::FilterNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "filter-" + std::to_string(plan_id_));

    auto filter = std::make_shared<PlanNode>(PlanNode::Type::FILTER);
    filter->name = "FILTER";
    if (node.child) {
        filter->child = plan_select(
            *dynamic_cast<analyzer::SelectNode*>(node.child.get())
        )->root;
    }
    plan->root = filter;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_sort(analyzer::SortNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "sort-" + std::to_string(plan_id_));

    auto sort = std::make_shared<PlanNode>(PlanNode::Type::SORT);
    sort->name = "SORT";
    if (node.child) {
        sort->child = plan_select(
            *dynamic_cast<analyzer::SelectNode*>(node.child.get())
        )->root;
    }
    plan->root = sort;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_limit(analyzer::LimitNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "limit-" + std::to_string(plan_id_));

    auto limit = std::make_shared<PlanNode>(PlanNode::Type::LIMIT);
    limit->name = "LIMIT";
    limit->limit = node.count;
    if (node.offset.has_value()) {
        limit->offset = node.offset.value();
    }
    if (node.child) {
        limit->child = plan_select(
            *dynamic_cast<analyzer::SelectNode*>(node.child.get())
        )->root;
    }
    plan->root = limit;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_table(analyzer::TableNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "table-" + std::to_string(plan_id_));

    auto scan = std::make_shared<PlanNode>(PlanNode::Type::SCAN);
    scan->name = node.database.empty() ? node.table
        : node.database + "." + node.table;
    plan->root = scan;
    return plan;
}

// Cost estimation
auto Planner::estimate_cost(std::shared_ptr<ExecutionPlan> plan) -> double {
    if (!plan || !plan->root) return 0.0;
    // Simple cost model: base cost per node
    double cost = 0.0;
    plan->walk([&cost](std::shared_ptr<PlanNode>) {
        cost += 1.0;
    });
    return cost;
}

// ── Join ordering heuristics ──

std::shared_ptr<analyzer::JoinNode> Planner::optimize_join_order(
    std::shared_ptr<analyzer::JoinNode> node) {
    // Placeholder — no optimization yet
    return node;
}

// ── Predicate pushdown ──

void Planner::push_down_predicates(std::shared_ptr<ExecutionPlan>& plan) {
    // Placeholder — no pushdown yet
}

// ── Index hint usage ──

std::optional<size_t> Planner::find_best_index(
    analyzer::TableNode& node,
    std::shared_ptr<analyzer::FilterNode> filter) {
    // Placeholder — no index support yet
    return std::nullopt;
}

} // namespace mnesso::planner
