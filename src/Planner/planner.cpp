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
            // Check if this is a DDL command (table name is "unknown" or special marker)
            if (table->table == "unknown" && table->database == "unknown") {
                // EXPLAIN command - create a generic DDL node
                auto node = std::make_shared<PlanNode>();
                node->node_type = PlanNode::Type::EXPLAIN;
                node->name = "explain";
                plan->root = node;
            } else if (table->table == "show_tables") {
                // SHOW TABLES command
                auto node = std::make_shared<PlanNode>();
                node->node_type = PlanNode::Type::SHOW;
                node->show_type = "TABLES";
                plan->root = node;
            } else if (table->table.empty() && !table->database.empty()) {
                // CREATE DATABASE command
                auto node = std::make_shared<PlanNode>();
                node->node_type = PlanNode::Type::CREATE;
                node->name = table->database;
                plan->root = node;
            } else if (!table->table.empty() && table->database == context_.current_database()) {
                // This could be CREATE TABLE or DROP TABLE - check if table exists
                // For now, treat as regular table scan if table exists, otherwise as DDL
                auto storage = context_.get_storage(table->table);
                if (storage) {
                    // Table exists - regular scan
                    plan->root = plan_table(*table)->root;
                } else {
                    // Table doesn't exist - assume it's CREATE TABLE
                    auto node = std::make_shared<PlanNode>();
                    node->node_type = PlanNode::Type::CREATE;
                    node->table_name = table->table;
                    node->columns = table->columns; // Pass column definitions
                    node->name = "create_table";
                    plan->root = node;
                }
            } else {
                // Regular table scan
                plan->root = plan_table(*table)->root;
            }
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
        if (node.from->node_type() == "Table") {
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
        filter->expression = node.where->node_type(); // Store expression type
        filter->child = scan;
        current = filter;
    }

    // Apply GROUP BY with aggregates
    if (!node.group_by.empty()) {
        auto agg = std::make_shared<PlanNode>(PlanNode::Type::GROUP_BY);
        agg->name = "GROUP BY";
        
        // Set up aggregate spec
        PlanNode::AggregateSpec agg_spec;
        for (const auto& col_expr : node.group_by) {
            agg_spec.group_by_columns.push_back(col_expr->node_type());
        }
        
        // Extract aggregate functions from SELECT columns
        for (const auto& col : node.columns) {
            if (col.expression && col.expression->node_type().find("Aggregate") != std::string::npos) {
                PlanNode::AggregateSpec::AggregateOp agg_op;
                agg_op.function_name = col.expression->node_type();
                agg_op.result_type = col.result_type;
                agg_op.is_distinct = false;
                agg_spec.aggregates.push_back(agg_op);
            }
        }
        
        agg->spec = agg_spec;
        agg->child = current;
        current = agg;
    }

    // Apply PROJECT (SELECT columns)
    auto project = std::make_shared<PlanNode>(PlanNode::Type::PROJECT);
    project->name = "SELECT";
    
    // Extract column names from SelectNode::ColumnExpr
    if (!node.columns.empty()) {
        for (const auto& col : node.columns) {
            if (!col.alias.empty()) {
                project->columns.push_back(col.alias);
            } else if (col.expression) {
                project->columns.push_back(col.expression->node_type());
            }
        }
    } else {
        project->columns.push_back("*");
    }
    
    project->child = current;
    plan->root = project;

    // Apply HAVING (after GROUP BY, before ORDER BY)
    if (node.having) {
        auto having = std::make_shared<PlanNode>(PlanNode::Type::FILTER);
        having->name = "HAVING";
        having->expression = node.having->node_type();
        having->child = plan->root;
        plan->root = having;
    }

    // Apply ORDER BY
    if (!node.order_by.empty()) {
        auto sort = std::make_shared<PlanNode>(PlanNode::Type::SORT);
        sort->name = "ORDER BY";
        for (const auto& [expr, desc] : node.order_by) {
            if (expr) {
                sort->order_by.push_back(expr->node_type());
            }
        }
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
    scan->table = node.table; // Set table field for SCAN processor
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
