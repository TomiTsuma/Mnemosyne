// src/Planner/planner.cpp — Query planner for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Planner/planner.h"
#include "Analyzer/analyzer.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::planner {

Planner::Planner(interpreters::Context& context) : context_{context} {}

auto Planner::plan(const AnalyzedQuery& query) -> Plan {
    Plan plan;

    switch (query.query_type) {
        case QueryAST::QueryType::SELECT:
            plan = plan_select(query);
            break;
        case QueryAST::QueryType::INSERT:
            plan = plan_insert(query);
            break;
        case QueryAST::QueryType::CREATE:
            plan = plan_create(query);
            break;
        case QueryAST::QueryType::DROP:
            plan = plan_drop(query);
            break;
        case QueryAST::QueryType::SHOW:
            plan = plan_show(query);
            break;
        case QueryAST::QueryType::DESCRIBE:
            plan = plan_describe(query);
            break;
        case QueryAST::QueryType::EXPLAIN:
            plan = plan_explain(query);
            break;
    }

    return plan;
}

auto Planner::plan_select(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::SELECT;

    // Create Scan node
    auto scan = std::make_shared<PlanNode>(PlanNode::Type::SCAN);
    scan->table = query.table;
    plan.root = scan;

    // Create Filter node if WHERE clause exists
    if (query.where.type != AnalyzedExpression::Type::NONE) {
        auto filter = std::make_shared<PlanNode>(PlanNode::Type::FILTER);
        filter->expression = query.where;
        filter->child = scan;
        scan->child = filter;
        plan.root = filter;
    }

    // Create Project node for columns
    auto project = std::make_shared<PlanNode>(PlanNode::Type::PROJECT);
    project->columns = query.columns;
    project->child = scan;
    if (query.where.type != AnalyzedExpression::Type::NONE) {
        project->child = filter_node(project->child);
    }
    plan.root = project;

    // Create GroupBy node if GROUP BY exists
    if (!query.group_by.empty()) {
        auto group_by = std::make_shared<PlanNode>(PlanNode::Type::GROUP_BY);
        group_by->columns = query.group_by;
        group_by->child = project;
        plan.root = group_by;
    }

    // Create Sort node if ORDER BY exists
    if (!query.order_by.empty()) {
        auto sort = std::make_shared<PlanNode>(PlanNode::Type::SORT);
        sort->order_by = query.order_by;
        sort->child = project;
        plan.root = sort;
    }

    // Create Limit node if LIMIT exists
    if (query.limit.second > 0) {
        auto limit = std::make_shared<PlanNode>(PlanNode::Type::LIMIT);
        limit->limit = query.limit.second;
        limit->offset = query.limit.first;
        limit->child = project;
        plan.root = limit;
    }

    return plan;
}

auto Planner::plan_insert(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::INSERT;
    auto insert = std::make_shared<PlanNode>(PlanNode::Type::INSERT);
    insert->table = query.table;
    insert->columns = query.insert_columns;
    insert->values = query.insert_values;
    plan.root = insert;
    return plan;
}

auto Planner::plan_create(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::CREATE;
    auto create = std::make_shared<PlanNode>(PlanNode::Type::CREATE);
    create->table_name = query.create_table_name;
    create->columns = query.create_columns;
    plan.root = create;
    return plan;
}

auto Planner::plan_drop(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::DROP;
    auto drop = std::make_shared<PlanNode>(PlanNode::Type::DROP);
    drop->table_name = query.drop_table_name;
    plan.root = drop;
    return plan;
}

auto Planner::plan_show(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::SHOW;
    auto show = std::make_shared<PlanNode>(PlanNode::Type::SHOW);
    show->show_type = query.show_type;
    plan.root = show;
    return plan;
}

auto Planner::plan_describe(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::DESCRIBE;
    auto describe = std::make_shared<PlanNode>(PlanNode::Type::DESCRIBE);
    describe->table_name = query.describe_table_name;
    plan.root = describe;
    return plan;
}

auto Planner::plan_explain(const AnalyzedQuery& query) -> Plan {
    Plan plan;
    plan.query_type = QueryAST::QueryType::EXPLAIN;
    auto explain = std::make_shared<PlanNode>(PlanNode::Type::EXPLAIN);
    explain->explain_plan = plan(query.explain_query);
    plan.root = explain;
    return plan;
}

auto Planner::filter_node(std::shared_ptr<PlanNode> node) -> std::shared_ptr<PlanNode> {
    // Helper to insert filter node
    return node;
}

} // namespace mnesso::planner