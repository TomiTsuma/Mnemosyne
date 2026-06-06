// src/Interpreters/block_interpreter.cpp — Block-level interpreter
// Mnemosyne: A column-oriented analytical DBMS

#include "Interpreters/blockInterpreter.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/interpreter_insert_query.h"
#include "Interpreters/interpreter_drop_query.h"
#include "Interpreters/interpreter_alter_query.h"
#include "Interpreters/interpreter_refresh_query.h"
#include "Interpreters/interpreter_select_query.h"
#include "Planner/execution_plan.h"
#include "Databases/database.h"
#include "Processors/processors_source.h"
#include "Processors/processor.h"
#include "Databases/database_manager.h"
#include "StorageUnits/storage_unit_catalog.h"
#include "StorageUnits/storage_unit_manager.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "Nodes/remote_executor.h"
#include "Common/exceptions.h"
#include "Analyzer/query_tree.h"
#include "DataTypes/data_type_factory.h"
#include "Columns/column_vector.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include "Columns/column_string.h"

namespace mnemo::interpreters {

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

        case planner::PlanNode::Type::EXCHANGE: {
            if (!node->table_name.empty()) {
                nodes::RemoteExecutor::execute_on_node(node->table_name, "SELECT 1");
            }
            if (!node->child) {
                throw common::Exception{
                    "BlockInterpreter: EXCHANGE node has no child",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            auto child_proc = proc_map[node->child];
            if (!child_proc) {
                throw common::Exception{
                    "BlockInterpreter: EXCHANGE child processor not found",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            return child_proc;
        }

        case planner::PlanNode::Type::INSERT:
        case planner::PlanNode::Type::CREATE:
        case planner::PlanNode::Type::DROP:
        case planner::PlanNode::Type::ALTER:
        case planner::PlanNode::Type::TRUNCATE:
        case planner::PlanNode::Type::DETACH:
        case planner::PlanNode::Type::SHOW:
        case planner::PlanNode::Type::DESCRIBE:
        case planner::PlanNode::Type::EXPLAIN:
        case planner::PlanNode::Type::USE:
        case planner::PlanNode::Type::REFRESH:
            // DDL / session commands — execute directly
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
            auto block = InterpreterCreateQuery::execute(context_, ddl_utils::query_from_plan(*node));
            result_.block = std::make_shared<core::Block>(std::move(block));
            break;
        }
        case planner::PlanNode::Type::INSERT: {
            auto block = InterpreterInsertQuery::execute(context_, ddl_utils::query_from_plan(*node));
            result_.block = std::make_shared<core::Block>(std::move(block));
            break;
        }
        case planner::PlanNode::Type::DROP:
        case planner::PlanNode::Type::TRUNCATE:
        case planner::PlanNode::Type::DETACH: {
            auto block = InterpreterDropQuery::execute(context_, ddl_utils::query_from_plan(*node));
            result_.block = std::make_shared<core::Block>(std::move(block));
            break;
        }
        case planner::PlanNode::Type::ALTER: {
            auto block = InterpreterAlterQuery::execute(context_, ddl_utils::query_from_plan(*node));
            result_.block = std::make_shared<core::Block>(std::move(block));
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
                    block->add_column("name", col);
                    result_.block = block;
                }
            } else if (node->show_type == "VIEWS" || node->show_type == "MATERIALIZED_VIEWS") {
                auto catalog = ddl_utils::require_catalog(context_);
                const auto names = node->show_type == "VIEWS"
                    ? catalog->view_names()
                    : catalog->materialized_view_names();
                auto col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < names.size(); ++i) {
                    col->insert_at(i, core::Field(names[i]));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", col);
                result_.block = block;
            } else if (node->show_type == "STORAGE_UNITS") {
                auto& mgr = storage_units::StorageUnitManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    type_col->insert_at(i, core::Field(storage_units::storage_unit_type_name(entries[i].type)));
                    status_col->insert_at(i, core::Field(storage_units::storage_unit_status_name(entries[i].status)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("status", status_col);
                result_.block = block;
            } else if (node->show_type == "STORAGE_USAGE") {
                auto& mgr = storage_units::StorageUnitManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto total_col = std::make_shared<columns::ColumnString>();
                auto used_col = std::make_shared<columns::ColumnString>();
                auto avail_col = std::make_shared<columns::ColumnString>();
                auto pct_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    mgr.refresh_stats(entries[i].name);
                    const auto* entry = mgr.get_unit(entries[i].name);
                    if (!entry) continue;
                    name_col->insert_at(i, core::Field(entry->name));
                    total_col->insert_at(i, core::Field(std::to_string(entry->capacity_bytes)));
                    used_col->insert_at(i, core::Field(std::to_string(entry->used_bytes)));
                    avail_col->insert_at(i, core::Field(std::to_string(entry->available_bytes)));
                    const double pct = entry->capacity_bytes > 0
                        ? (100.0 * static_cast<double>(entry->used_bytes)
                           / static_cast<double>(entry->capacity_bytes))
                        : 0.0;
                    pct_col->insert_at(i, core::Field(std::to_string(pct)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("total_bytes", total_col);
                block->add_column("used_bytes", used_col);
                block->add_column("available_bytes", avail_col);
                block->add_column("used_pct", pct_col);
                result_.block = block;
            } else if (node->show_type == "NODES") {
                auto& mgr = nodes::NodeManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto role_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto host_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].node_name));
                    type_col->insert_at(i, core::Field(nodes::node_type_name(entries[i].node_type)));
                    role_col->insert_at(i, core::Field(nodes::node_role_name(entries[i].node_role)));
                    status_col->insert_at(i, core::Field(nodes::node_status_name(entries[i].status)));
                    host_col->insert_at(i, core::Field(entries[i].host));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("role", role_col);
                block->add_column("status", status_col);
                block->add_column("host", host_col);
                result_.block = block;
            } else if (node->show_type == "NODE_METRICS") {
                auto& mgr = nodes::NodeManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto cpu_col = std::make_shared<columns::ColumnString>();
                auto mem_col = std::make_shared<columns::ColumnString>();
                auto q_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (!node->table_name.empty() && entries[i].node_name != node->table_name) {
                        continue;
                    }
                    name_col->insert(core::Field(entries[i].node_name));
                    cpu_col->insert(core::Field(std::to_string(entries[i].metrics.cpu_utilization_pct)));
                    mem_col->insert(core::Field(std::to_string(entries[i].metrics.memory_used_bytes)));
                    q_col->insert(core::Field(std::to_string(entries[i].metrics.query_throughput)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("cpu_utilization_pct", cpu_col);
                block->add_column("memory_used_bytes", mem_col);
                block->add_column("query_throughput", q_col);
                result_.block = block;
            } else if (node->show_type == "NODE_CAPABILITIES") {
                auto& mgr = nodes::NodeManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto cap_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (!node->table_name.empty() && entries[i].node_name != node->table_name) {
                        continue;
                    }
                    name_col->insert(core::Field(entries[i].node_name));
                    std::string caps;
                    for (size_t j = 0; j < entries[i].capabilities.size(); ++j) {
                        if (j > 0) caps += ",";
                        caps += entries[i].capabilities[j];
                    }
                    cap_col->insert(core::Field(caps));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("capabilities", cap_col);
                result_.block = block;
            } else if (node->show_type == "NODE_PARTITIONS") {
                auto& mgr = nodes::NodeManager::instance();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto part_col = std::make_shared<columns::ColumnString>();
                const auto entries = mgr.list_entries();
                for (const auto& entry : entries) {
                    if (!node->table_name.empty() && entry.node_name != node->table_name) {
                        continue;
                    }
                    for (const auto& pid : entry.partition_ids) {
                        name_col->insert(core::Field(entry.node_name));
                        part_col->insert(core::Field(pid));
                    }
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("node", name_col);
                block->add_column("partition_id", part_col);
                result_.block = block;
            } else if (node->show_type == "NODE_REPLICAS") {
                auto& mgr = nodes::NodeManager::instance();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto rep_col = std::make_shared<columns::ColumnString>();
                const auto entries = mgr.list_entries();
                for (const auto& entry : entries) {
                    if (!node->table_name.empty() && entry.node_name != node->table_name) {
                        continue;
                    }
                    for (const auto& rid : entry.replica_ids) {
                        name_col->insert(core::Field(entry.node_name));
                        rep_col->insert(core::Field(rid));
                    }
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("node", name_col);
                block->add_column("replica_id", rep_col);
                result_.block = block;
            } else if (node->show_type == "CLUSTERS") {
                auto& mgr = nodes::NodeManager::instance();
                const auto clusters = mgr.list_clusters();
                auto name_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < clusters.size(); ++i) {
                    name_col->insert_at(i, core::Field(clusters[i].name));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                result_.block = block;
            }
            break;
        }
        case planner::PlanNode::Type::DESCRIBE: {
            std::shared_ptr<storages::IStorage> storage;
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Node) {
                auto& mgr = nodes::NodeManager::instance();
                const auto* entry = mgr.get_node(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown node: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("node_id", entry->node_id);
                add_row("node_name", entry->node_name);
                add_row("cluster_id", entry->cluster_id);
                add_row("type", nodes::node_type_name(entry->node_type));
                add_row("role", nodes::node_role_name(entry->node_role));
                add_row("status", nodes::node_status_name(entry->status));
                add_row("host", entry->host);
                add_row("port", std::to_string(entry->port));
                add_row("version", entry->version);
                add_row("cpu_cores", std::to_string(entry->resources.cpu_cores));
                add_row("memory_bytes", std::to_string(entry->resources.memory_bytes));
                add_row("gpu_count", std::to_string(entry->resources.gpu_count));
                add_row("storage_bytes", std::to_string(entry->resources.storage_bytes));
                std::string caps;
                for (size_t j = 0; j < entry->capabilities.size(); ++j) {
                    if (j > 0) caps += ",";
                    caps += entry->capabilities[j];
                }
                add_row("capabilities", caps);
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Cluster) {
                auto& mgr = nodes::NodeManager::instance();
                const auto* cluster = mgr.get_cluster(node->table_name);
                if (!cluster) {
                    throw common::Exception{
                        "Unknown cluster: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                field_col->insert(core::Field("name"));
                value_col->insert(core::Field(cluster->name));
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::StorageUnit) {
                auto& mgr = storage_units::StorageUnitManager::instance();
                const auto* entry = mgr.get_unit(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown storage unit: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                mgr.refresh_stats(node->table_name);
                entry = mgr.get_unit(node->table_name);
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("type", storage_units::storage_unit_type_name(entry->type));
                add_row("status", storage_units::storage_unit_status_name(entry->status));
                add_row("path", entry->path);
                add_row("endpoint", entry->endpoint);
                add_row("bucket", entry->bucket);
                add_row("region", entry->region);
                add_row("capacity_bytes", std::to_string(entry->capacity_bytes));
                add_row("used_bytes", std::to_string(entry->used_bytes));
                add_row("available_bytes", std::to_string(entry->available_bytes));
                add_row("reference_count", std::to_string(entry->reference_count));
                const auto caps = storage_units::capabilities_for_type(entry->type);
                add_row("capabilities", caps.empty() ? "" : caps.front());
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::View) {
                auto catalog = ddl_utils::require_catalog(context_);
                auto view = catalog->get_view(node->table_name);
                if (!view) {
                    throw common::Exception{
                        "Unknown view: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                parsers::QueryAST nested;
                nested.query_type = parsers::QueryAST::QueryType::SELECT;
                nested.select = view->definition;
                auto block = InterpreterSelectQuery::execute(context_, nested);
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                for (const auto& col_name : block.column_names()) {
                    name_col->insert(core::Field(col_name));
                    if (auto col = block.get_column(col_name)) {
                        type_col->insert(core::Field(col->get_data_type()->name()));
                    } else {
                        type_col->insert(core::Field(std::string{"String"}));
                    }
                }
                auto out = std::make_shared<core::Block>();
                out->add_column("name", name_col);
                out->add_column("type", type_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::MaterializedView) {
                storage = ddl_utils::resolve_storage(context_, node->table_name);
            } else {
                storage = ddl_utils::resolve_storage(context_, node->table_name);
            }
            if (storage) {
                auto columns = storage->columns();
                auto col_types = storage->column_types();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                for (const auto& col_name : columns) {
                    name_col->insert(core::Field(col_name));
                    type_col->insert(core::Field(col_types.at(col_name)->name()));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                result_.block = block;
            }
            break;
        }
        case planner::PlanNode::Type::REFRESH: {
            parsers::QueryAST query;
            query.query_type = parsers::QueryAST::QueryType::REFRESH;
            query.refresh.name = node->refresh_name;
            auto block = InterpreterRefreshQuery::execute(context_, query);
            result_.block = std::make_shared<core::Block>(std::move(block));
            break;
        }
        case planner::PlanNode::Type::USE: {
            const std::string& target = node->name;
            if (!context_.get_database(target)) {
                throw common::Exception{
                    "Unknown database: " + target,
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            context_.set_current_database(target);
            result_.block = std::make_shared<core::Block>(ddl_utils::make_ok_block());
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

} // namespace mnemo::interpreters
