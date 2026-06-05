// src/Interpreters/interpreter.cpp — SQL interpreter for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Interpreters/interpreter.h"
#include "Interpreters/context.h"
#include "Databases/database_memory.h"
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Planner/execution_plan.h"
#include "Parsers/lexer.h"
#include "Parsers/parser.h"
#include "Parsers/parser_query.h"
#include "Processors/processor.h"
#include "Processors/processors.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::interpreters {

using planner::PlanNode;

// ── Interpreter ──

Interpreter::Interpreter(databases::DatabaseManager& db_manager)
    : db_manager_{db_manager} {}

auto Interpreter::execute(std::string_view query_text) -> core::Block {
    // Parse — use QueryParser (concrete subclass of Parser)
    auto lexer = parsers::Lexer{std::string{query_text}};
    auto parser = std::make_unique<parsers::QueryParser>(std::move(lexer));
    auto query_ast = parser->parse();  // std::unique_ptr<QueryAST>

    // Analyze
    auto db = db_manager_.get_database("default");
    if (!db) {
        db = db_manager_.create_database("default");
    }

    auto context = std::make_shared<Context>(db);
    auto analyzer = analyzer::Analyzer{*context};
    auto analyzed_query = analyzer.analyze(std::shared_ptr<parsers::QueryAST>{std::move(query_ast)});

    // Context + Plan (context must precede planner — Planner takes Context&)
    auto planner = planner::Planner{*context};
    auto query_tree = analyzer.buildQueryTree(analyzed_query);
    auto plan = planner.plan(query_tree);

    // Execute
    auto processor = create_processor(plan, context);
    processor->start();
    auto result = processor->result();

    return result.value_or(core::Block{});
}

auto Interpreter::create_processor(Plan plan,
                                    std::shared_ptr<Context> context)
    -> std::shared_ptr<processors::Processor> {
    auto root = plan.root;
    if (!root) {
        throw common::Exception{
            "Interpreter: no plan root",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    switch (root->node_type) {
        case PlanNode::Type::SCAN: {
            auto scan_proc = std::make_shared<processors::SimpleScanProcessor>(root->table, *context);
            return scan_proc;
        }
        case PlanNode::Type::FILTER: {
            auto filter_proc = std::make_shared<processors::SimpleFilterProcessor>(
                core::Block{}, root->expression);
            return filter_proc;
        }
        case PlanNode::Type::PROJECT: {
            auto project_proc = std::make_shared<processors::SimpleProjectProcessor>(
                core::Block{}, root->columns);
            return project_proc;
        }
        case PlanNode::Type::GROUP_BY: {
            auto group_by_proc = std::make_shared<processors::SimpleGroupByProcessor>(
                core::Block{});
            return group_by_proc;
        }
        case PlanNode::Type::SORT: {
            auto sort_proc = std::make_shared<processors::SimpleSortProcessor>(
                core::Block{}, root->order_by);
            return sort_proc;
        }
        case PlanNode::Type::LIMIT: {
            auto limit_proc = std::make_shared<processors::SimpleLimitProcessor>(
                core::Block{}, root->offset, root->limit);
            return limit_proc;
        }
        case PlanNode::Type::INSERT: {
            auto insert_proc = std::make_shared<processors::InsertProcessor>(
                root->table, root->columns, root->values, *context);
            return insert_proc;
        }
        case PlanNode::Type::CREATE: {
            // Check if this is CREATE DATABASE or CREATE TABLE
            // CREATE DATABASE sets node->name, CREATE TABLE sets node->table_name
            bool is_database = !root->name.empty() && root->table_name.empty();
            std::string target_name = is_database ? root->name : root->table_name;
            auto create_proc = std::make_shared<processors::CreateProcessor>(
                target_name, root->columns, is_database, *context);
            return create_proc;
        }
        case PlanNode::Type::DROP: {
            auto drop_proc = std::make_shared<processors::DropProcessor>(
                root->table_name, *context);
            return drop_proc;
        }
        case PlanNode::Type::SHOW: {
            auto show_proc = std::make_shared<processors::ShowProcessor>(
                root->show_type, *context);
            return show_proc;
        }
        case PlanNode::Type::DESCRIBE: {
            auto describe_proc = std::make_shared<processors::DescribeProcessor>(
                root->table_name, *context);
            return describe_proc;
        }
        case PlanNode::Type::EXPLAIN: {
            auto explain_proc = std::make_shared<processors::ExplainProcessor>(
                root->explain_plan, *context);
            return explain_proc;
        }
        default:
            throw common::Exception{
                "Interpreter: unknown plan node type",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
}

} // namespace mnesso::interpreters