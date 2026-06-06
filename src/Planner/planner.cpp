// src/Planner/planner.cpp — Query planner for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Planner/planner.h"
#include "Analyzer/query_tree.h"
#include "Nodes/node_manager.h"
#include "Nodes/node_scheduler.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::planner {

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
    } else if (type == "DDL") {
        auto* ddl = dynamic_cast<analyzer::DDLNode*>(tree.get());
        if (ddl) {
            plan->root = plan_ddl(*ddl)->root;
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

    if (nodes::NodeManager::instance().list_entries().size() > 1) {
        if (auto remote = nodes::NodeScheduler::instance().pick_remote_node({"QUERY_ENGINE"})) {
            auto exchange = std::make_shared<PlanNode>(PlanNode::Type::EXCHANGE);
            exchange->name = "EXCHANGE";
            exchange->table_name = *remote;
            exchange->child = plan->root;
            plan->root = exchange;
        }
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
    scan->table = node.table;
    plan->root = scan;
    return plan;
}

std::shared_ptr<ExecutionPlan> Planner::plan_ddl(analyzer::DDLNode& node) {
    auto plan = std::make_shared<ExecutionPlan>(++plan_id_, "ddl-" + std::to_string(plan_id_));
    auto plan_node = std::make_shared<PlanNode>();

    switch (node.kind) {
        case analyzer::DDLNode::Kind::CreateDatabase:
            plan_node->node_type = PlanNode::Type::CREATE;
            plan_node->name = node.database;
            plan_node->if_not_exists = node.if_not_exists;
            break;
        case analyzer::DDLNode::Kind::CreateTable:
            plan_node->node_type = PlanNode::Type::CREATE;
            plan_node->table_name = node.table;
            plan_node->if_not_exists = node.if_not_exists;
            plan_node->engine = node.engine;
            plan_node->column_defs = node.column_defs;
            for (const auto& col : node.column_defs) {
                plan_node->columns.push_back(col.name);
            }
            break;
        case analyzer::DDLNode::Kind::Insert:
            plan_node->node_type = PlanNode::Type::INSERT;
            plan_node->table = node.table;
            plan_node->columns = node.insert_columns;
            plan_node->values = node.insert_values;
            break;
        case analyzer::DDLNode::Kind::Drop:
            plan_node->node_type = PlanNode::Type::DROP;
            plan_node->table_name = node.table;
            plan_node->if_exists = node.if_exists;
            plan_node->drop_kind = parsers::QueryAST::Drop::Kind::Drop;
            break;
        case analyzer::DDLNode::Kind::Truncate:
            plan_node->node_type = PlanNode::Type::TRUNCATE;
            plan_node->table_name = node.table;
            plan_node->if_exists = node.if_exists;
            plan_node->drop_kind = parsers::QueryAST::Drop::Kind::Truncate;
            break;
        case analyzer::DDLNode::Kind::Detach:
            plan_node->node_type = PlanNode::Type::DETACH;
            plan_node->table_name = node.table;
            plan_node->if_exists = node.if_exists;
            plan_node->drop_kind = parsers::QueryAST::Drop::Kind::Detach;
            break;
        case analyzer::DDLNode::Kind::Alter:
            plan_node->node_type = PlanNode::Type::ALTER;
            plan_node->table_name = node.table;
            plan_node->alter_commands = node.alter_commands;
            break;
        case analyzer::DDLNode::Kind::ShowDatabases:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "DATABASES";
            break;
        case analyzer::DDLNode::Kind::ShowTables:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "TABLES";
            break;
        case analyzer::DDLNode::Kind::ShowViews:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "VIEWS";
            break;
        case analyzer::DDLNode::Kind::ShowMaterializedViews:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "MATERIALIZED_VIEWS";
            break;
        case analyzer::DDLNode::Kind::ShowStorageUnits:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "STORAGE_UNITS";
            break;
        case analyzer::DDLNode::Kind::ShowStorageUsage:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "STORAGE_USAGE";
            break;
        case analyzer::DDLNode::Kind::ShowNodes:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "NODES";
            break;
        case analyzer::DDLNode::Kind::ShowNodeMetrics:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "NODE_METRICS";
            plan_node->table_name = node.show_node_name;
            break;
        case analyzer::DDLNode::Kind::ShowNodeCapabilities:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "NODE_CAPABILITIES";
            plan_node->table_name = node.show_node_name;
            break;
        case analyzer::DDLNode::Kind::ShowNodePartitions:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "NODE_PARTITIONS";
            plan_node->table_name = node.show_node_name;
            break;
        case analyzer::DDLNode::Kind::ShowNodeReplicas:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "NODE_REPLICAS";
            plan_node->table_name = node.show_node_name;
            break;
        case analyzer::DDLNode::Kind::ShowClusters:
            plan_node->node_type = PlanNode::Type::SHOW;
            plan_node->show_type = "CLUSTERS";
            break;
        case analyzer::DDLNode::Kind::Describe:
            plan_node->node_type = PlanNode::Type::DESCRIBE;
            plan_node->table_name = node.table;
            plan_node->describe_object_kind = node.describe_kind;
            break;
        case analyzer::DDLNode::Kind::Explain:
            plan_node->node_type = PlanNode::Type::EXPLAIN;
            break;
        case analyzer::DDLNode::Kind::Use:
            plan_node->node_type = PlanNode::Type::USE;
            plan_node->name = node.use_database;
            break;
        case analyzer::DDLNode::Kind::Refresh:
            plan_node->node_type = PlanNode::Type::REFRESH;
            plan_node->refresh_name = node.refresh_name;
            break;
    }

    plan->root = plan_node;
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

} // namespace mnemo::planner
