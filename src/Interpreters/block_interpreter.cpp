// src/Interpreters/block_interpreter.cpp — Block-level interpreter
// Mnemosyne: A column-oriented analytical DBMS

#include "Interpreters/blockInterpreter.h"
#include "Interpreters/context.h"
#include "Planner/execution_plan.h"
#include "Processors/processors_source.h"
#include "Processors/processor.h"
#include "Databases/database_manager.h"
#include "Common/exceptions.h"
#include "Analyzer/query_tree.h"
#include "DataTypes/data_type_factory.h"
#include "Columns/column_vector.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include "Columns/column_string.h"

namespace mnesso::interpreters {

// ── BlockInterpreter ──

BlockInterpreter::BlockInterpreter(std::shared_ptr<planner::ExecutionPlan> plan,
                                   Context& context)
    : plan_(std::move(plan)), context_(context) {}

auto BlockInterpreter::execute() -> QueryResult {
    auto start = std::chrono::high_resolution_clock::now();

    try {
        pipeline_ = build_pipeline();
        if (!pipeline_) {
            result_.error = "Failed to build pipeline";
            return result_;
        }

        // Start the pipeline
        pipeline_->start();

        // Read all blocks from the pipeline
        auto outputs = pipeline_->outputs();
        if (outputs.empty()) {
            // For DDL commands (CREATE, DROP, etc.), no output is expected
            // Return an empty block to indicate success only if we didn't pre-populate it
            if (!result_.block) {
                result_.block = std::make_shared<core::Block>(core::Block{});
            }
            return result_;
        }

        // Get header
        auto header = outputs[0]->getHeader();

        // Read all data blocks
        auto result_block = std::make_shared<core::Block>(core::Block{});
        
        // For processors that use NoOpOutputStream, we need to access the data differently
        // The processors store their result in current_block_ after start()
        // We'll extract it from the pipeline's root processor
        
        // Try to get the result from the pipeline
        // This is a simplified approach - in a full implementation, we'd stream blocks
        if (auto* scan_proc = dynamic_cast<processors::ScanProcessor*>(pipeline_.get())) {
            // Scan processor - read from it
            result_block = std::make_shared<core::Block>(scan_proc->getHeader());
        } else if (plan_ && plan_->root) {
            // For other processors, we need to execute the pipeline and collect results
            // This is a placeholder - the full implementation would properly stream blocks
            result_block = std::make_shared<core::Block>(header);
        }

        if (!result_.block) {
            result_.block = result_block;
        }

    } catch (const common::Exception& e) {
        result_.error = e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result_.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return result_;
}

auto BlockInterpreter::has_result() const -> bool {
    return result_.block != nullptr;
}

// ── build_pipeline — traverse PlanNode DAG and create processor chain ──

auto BlockInterpreter::build_pipeline() -> std::shared_ptr<processors::Processor> {
    if (!plan_ || !plan_->root) {
        return nullptr;
    }

    // Process from bottom (leaves) to top (root) using post-order traversal
    std::unordered_map<std::shared_ptr<planner::PlanNode>, std::shared_ptr<processors::Processor>> proc_map;

    // Helper function to create processors in post-order (children before parents)
    std::function<void(std::shared_ptr<planner::PlanNode>)> create_processors_postorder =
        [&](std::shared_ptr<planner::PlanNode> node) {
            if (!node) return;

            // Recursively create child processors first
            if (node->child) {
                create_processors_postorder(node->child);
            }
            for (auto& child : node->children) {
                create_processors_postorder(child);
            }

            // Create processor for this node
            auto proc = create_processor_for_node(node, proc_map);
            if (!proc) {
                throw common::Exception{
                    "BlockInterpreter: failed to create processor for node type: " +
                    std::to_string(static_cast<int>(node->node_type)),
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            proc_map[node] = proc;
        };

    // Create processors starting from root
    create_processors_postorder(plan_->root);

    return proc_map[plan_->root];
}

// ── create_processor_for_node — create a processor for a single PlanNode ──

auto BlockInterpreter::create_processor_for_node(
    std::shared_ptr<planner::PlanNode> node,
    std::unordered_map<std::shared_ptr<planner::PlanNode>, std::shared_ptr<processors::Processor>>& proc_map)
    -> std::shared_ptr<processors::Processor> {

    switch (node->node_type) {
        case planner::PlanNode::Type::SCAN: {
            // Create ScanProcessor from storage
            auto storage = context_.get_storage(node->table);
            if (!storage) {
                throw common::Exception{
                    "BlockInterpreter: unknown table: " + node->table,
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto scan = std::make_shared<processors::ScanProcessor>(
                storage, node->columns.empty() ? storage->columns() : node->columns);

            // Scan has no inputs (it's a source)
            return scan;
        }

        case planner::PlanNode::Type::FILTER: {
            // Create FilterProcessor
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: FILTER node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: FILTER child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            // Build predicate from expression
            auto predicate = build_predicate(node->expression);
            
            // Create filter processor (stream connections handled during execution)
            auto filter = std::make_shared<processors::FilterProcessor>(
                nullptr, predicate);

            return filter;
        }

        case planner::PlanNode::Type::PROJECT: {
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: PROJECT node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: PROJECT child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto columns = node->columns;
            if (columns.empty() || (columns.size() == 1 && columns[0] == "*")) {
                // Project all columns from header
                columns = {"*"};
            }

            // Create project processor (stream connections handled during execution)
            auto project = std::make_shared<processors::ProjectProcessor>(
                nullptr, columns);

            return project;
        }

        case planner::PlanNode::Type::GROUP_BY: {
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: GROUP_BY node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: GROUP_BY child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto group_by = node->columns;

            // Extract aggregate specs from variant
            std::vector<std::pair<std::string, std::vector<std::string>>> agg_list;
            if (auto* agg_spec = std::get_if<planner::PlanNode::AggregateSpec>(&node->spec)) {
                for (auto& agg : agg_spec->aggregates) {
                    agg_list.push_back({agg.function_name, agg.input_columns});
                }
            }

            // Create groupby processor (stream connections handled during execution)
            auto groupby = std::make_shared<processors::GroupByProcessor>(
                nullptr, group_by, agg_list);

            return groupby;
        }

        case planner::PlanNode::Type::SORT: {
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: SORT node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: SORT child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            // Build descending flags (default ASC)
            std::vector<bool> descending(node->order_by.size(), false);
            
            // Create sort processor (stream connections handled during execution)
            auto sort = std::make_shared<processors::SortProcessor>(
                nullptr, node->order_by, descending);

            return sort;
        }

        case planner::PlanNode::Type::LIMIT: {
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: LIMIT node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: LIMIT child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }

            // Create limit processor (stream connections handled during execution)
            auto limit = std::make_shared<processors::LimitProcessor>(
                nullptr, node->limit, node->offset);

            return limit;
        }

        case planner::PlanNode::Type::INSERT:
        case planner::PlanNode::Type::CREATE:
        case planner::PlanNode::Type::DROP:
        case planner::PlanNode::Type::SHOW:
        case planner::PlanNode::Type::DESCRIBE:
        case planner::PlanNode::Type::EXPLAIN:
            // DDL commands — execute directly
            execute_ddl_command(node);
            return std::make_shared<processors::EmptyBlockSource>();

        default:
            throw common::Exception{
                "BlockInterpreter: unknown node type: " +
                std::to_string(static_cast<int>(node->node_type)),
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
}

// ── execute_ddl_command — execute DDL commands directly ──

void BlockInterpreter::execute_ddl_command(std::shared_ptr<planner::PlanNode> node) {
    if (!node) return;

    switch (node->node_type) {
        case planner::PlanNode::Type::CREATE: {
            // CREATE DATABASE or CREATE TABLE
            if (!node->table_name.empty()) {
                // CREATE TABLE
                auto storage = context_.get_storage(node->table_name);
                if (!storage) {
                    // Create new table in current database
                    auto current_db = context_.current_database();
                    if (current_db.empty()) {
                        // If no current database, use "default" or the first available
                        current_db = "default";
                        context_.set_current_database(current_db);
                    }
                    auto db = context_.get_database(current_db);
                    if (!db) {
                        // If database doesn't exist, create it
                        auto& db_manager = databases::DatabaseManager::instance();
                        db_manager.create_database(current_db);
                        db = db_manager.get_database(current_db);
                        if (db) {
                            context_.register_database(current_db, db);
                        }
                    }
                    if (db) {
                        // Create table with specified columns
                        std::unordered_map<std::string, datatypes::DataTypePtr> columns;
                        for (auto& col : node->columns) {
                            // Default to String type for now
                            columns[col] = datatypes::get_data_type("String");
                        }
                        db->create_table(node->table_name, columns, "Memory");
                        
                        // Register the storage in context
                        auto new_storage = db->table(node->table_name);
                        if (new_storage) {
                            context_.register_storage(node->table_name, new_storage);
                        }
                    }
                }
            } else {
                // CREATE DATABASE
                auto& db_manager = databases::DatabaseManager::instance();
                db_manager.create_database(node->name);
                // Register the database in context
                auto db = db_manager.get_database(node->name);
                if (db) {
                    context_.register_database(node->name, db);
                }
            }
            break;
        }
        case planner::PlanNode::Type::DROP: {
            // DROP TABLE
            auto db = context_.get_database(context_.current_database());
            if (db && !node->table_name.empty()) {
                db->drop_table(node->table_name);
            }
            break;
        }
        case planner::PlanNode::Type::SHOW: {
            // SHOW DATABASES or SHOW TABLES
            if (node->show_type == "DATABASES") {
                auto db_names = context_.databases();
                auto col = std::make_shared<columns::ColumnString>();
                for (size_t i=0; i < db_names.size(); i++){
                    col->insert_at(i, core::Field(db_names[i]));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("databases",col);
                result_.block = block;
            } else if (node->show_type == "TABLES") {
                auto db = context_.get_database(context_.current_database());
                if (db) {
                    auto table_names = db->tables();
                    auto col = std::make_shared<columns::ColumnString>();
                    for (size_t i=0; i < table_names.size(); i++){
                        col->insert_at(i, core::Field(table_names[i]));
                    }
                    auto block = std::make_shared<core::Block>();
                    block->add_column("tables", col);
                    result_.block = block;
                }
            }
            break;
        }
        case planner::PlanNode::Type::DESCRIBE: {
            // DESCRIBE TABLE
            auto storage = context_.get_storage(node->table_name);
            if (storage) {
                auto columns = storage->columns();
                auto col_types = storage->column_types();
                // Results would be returned via result_.block
            }
            break;
        }
        case planner::PlanNode::Type::INSERT: {
            // INSERT INTO
            auto storage = context_.get_storage(node->table);
            if (storage) {
                // For now, skip INSERT execution to avoid type conversion issues
                // This needs proper type handling based on column types
                (void)storage;
            }
            break;
        }
        default:
            break;
    }
}

// ── build_predicate — convert expression string to predicate function ──

std::function<bool(const core::Field&)> BlockInterpreter::build_predicate(
    const std::string& expr) {
    // Simple predicate builder — parses "column = value" style expressions
    auto pos = expr.find('=');
    if (pos == std::string::npos) {
        // No condition — always true
        return [](const core::Field&) { return true; };
    }

    auto col_name = expr.substr(0, pos);
    auto value_str = expr.substr(pos + 1);

    // Trim whitespace
    while (!col_name.empty() && col_name.back() == ' ') col_name.pop_back();
    while (!value_str.empty() && value_str.front() == ' ') value_str.erase(0, 1);

    // Remove quotes from value
    if (value_str.size() >= 2 && value_str.front() == '\'' && value_str.back() == '\'') {
        value_str = value_str.substr(1, value_str.size() - 2);
    }

    return [col_name, value_str](const core::Field& field) -> bool {
        // Simple equality check
        return std::visit([&](auto&& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v == (value_str == "1" || value_str == "true");
            } else if constexpr (std::is_integral_v<T>) {
                try {
                    return v == std::stoll(value_str);
                } catch (...) {
                    return false;
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                try {
                    return v == std::stod(value_str);
                } catch (...) {
                    return false;
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v == value_str;
            } else {
                // For other types (monostate, shared_ptr, etc.), return false
                return false;
            }
        }, field.variant());
    };
}

} // namespace mnesso::interpreters
