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
#include "ReplicaGroups/replica_group_catalog.h"
#include "ReplicaGroups/replica_group_manager.h"
#include "ShardGroups/shard_group_catalog.h"
#include "ShardGroups/shard_group_manager.h"
#include "Connectors/connector_catalog.h"
#include "Connectors/connector_manager.h"
#include "Pipelines/pipeline_catalog.h"
#include "Pipelines/pipeline_manager.h"
#include "Streaming/stream_catalog.h"
#include "Streaming/stream_manager.h"
#include "Models/model_manager.h"
#include "Models/model_catalog.h"
#include "FeatureSets/feature_set_manager.h"
#include "FeatureSets/feature_set_catalog.h"
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
            } else if (node->show_type == "CONNECTORS") {
                auto& mgr = connectors::ConnectorManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    type_col->insert_at(i, core::Field(connectors::connector_type_name(entries[i].type)));
                    status_col->insert_at(i, core::Field(connectors::connector_status_name(entries[i].status)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("status", status_col);
                result_.block = block;
            } else if (node->show_type == "CONNECTOR_CAPABILITIES") {
                auto& mgr = connectors::ConnectorManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto cap_col = std::make_shared<columns::ColumnString>();
                for (const auto& entry : entries) {
                    if (!node->table_name.empty() && entry.name != node->table_name) {
                        continue;
                    }
                    for (const auto& cap : entry.capabilities) {
                        name_col->insert(core::Field(entry.name));
                        cap_col->insert(core::Field(cap));
                    }
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("capability", cap_col);
                result_.block = block;
            } else if (node->show_type == "CONNECTOR_STATUS") {
                auto& mgr = connectors::ConnectorManager::instance();
                const auto entries = mgr.list_entries();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto ok_col = std::make_shared<columns::ColumnString>();
                auto latency_col = std::make_shared<columns::ColumnString>();
                auto message_col = std::make_shared<columns::ColumnString>();
                for (const auto& entry : entries) {
                    if (!node->table_name.empty() && entry.name != node->table_name) {
                        continue;
                    }
                    name_col->insert(core::Field(entry.name));
                    status_col->insert(core::Field(connectors::connector_status_name(entry.status)));
                    ok_col->insert(core::Field(std::string{entry.last_test.ok ? "true" : "false"}));
                    latency_col->insert(core::Field(std::to_string(entry.last_test.latency_ms)));
                    message_col->insert(core::Field(entry.last_test.message));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("status", status_col);
                block->add_column("last_test_ok", ok_col);
                block->add_column("last_test_latency_ms", latency_col);
                block->add_column("last_test_message", message_col);
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
            } else if (node->show_type == "REPLICA_GROUPS") {
                auto& mgr = replica_groups::ReplicaGroupManager::instance();
                mgr.refresh_lag();
                const auto groups = mgr.list_groups();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto factor_col = std::make_shared<columns::ColumnString>();
                auto consistency_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < groups.size(); ++i) {
                    name_col->insert_at(i, core::Field(groups[i].name));
                    factor_col->insert_at(i, core::Field(std::to_string(groups[i].replication_factor)));
                    consistency_col->insert_at(
                        i, core::Field(replica_groups::consistency_mode_name(groups[i].consistency_mode)));
                    status_col->insert_at(
                        i, core::Field(replica_groups::replica_group_status_name(groups[i].status)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("replication_factor", factor_col);
                block->add_column("consistency", consistency_col);
                block->add_column("status", status_col);
                result_.block = block;
            } else if (node->show_type == "SHARD_GROUPS") {
                auto& mgr = shard_groups::ShardGroupManager::instance();
                const auto groups = mgr.list_groups();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto count_col = std::make_shared<columns::ColumnString>();
                auto key_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < groups.size(); ++i) {
                    name_col->insert_at(i, core::Field(groups[i].name));
                    type_col->insert_at(
                        i, core::Field(shard_groups::shard_strategy_name(groups[i].strategy)));
                    count_col->insert_at(i, core::Field(std::to_string(groups[i].shard_count)));
                    key_col->insert_at(i, core::Field(groups[i].shard_key));
                    status_col->insert_at(
                        i, core::Field(shard_groups::shard_group_status_name(groups[i].status)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("shard_count", count_col);
                block->add_column("shard_key", key_col);
                block->add_column("status", status_col);
                result_.block = block;
            } else if (node->show_type == "SHARDS" || node->show_type == "SHARD_STATUS") {
                auto& mgr = shard_groups::ShardGroupManager::instance();
                const auto members = mgr.list_all_members();
                auto group_col = std::make_shared<columns::ColumnString>();
                auto shard_col = std::make_shared<columns::ColumnString>();
                auto node_col = std::make_shared<columns::ColumnString>();
                auto state_col = std::make_shared<columns::ColumnString>();
                auto rows_col = std::make_shared<columns::ColumnString>();
                auto size_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < members.size(); ++i) {
                    group_col->insert_at(i, core::Field(members[i].group_name));
                    shard_col->insert_at(i, core::Field(members[i].shard_id));
                    node_col->insert_at(i, core::Field(members[i].node_id));
                    state_col->insert_at(
                        i, core::Field(shard_groups::shard_state_name(members[i].state)));
                    rows_col->insert_at(i, core::Field(std::to_string(members[i].row_count)));
                    size_col->insert_at(i, core::Field(std::to_string(members[i].size_bytes)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("shard_group", group_col);
                block->add_column("shard_id", shard_col);
                block->add_column("node_id", node_col);
                block->add_column("state", state_col);
                if (node->show_type == "SHARD_STATUS") {
                    block->add_column("row_count", rows_col);
                    block->add_column("size_bytes", size_col);
                }
                result_.block = block;
            } else if (node->show_type == "PIPELINES") {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto entries = mgr.list_pipelines();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto owner_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    status_col->insert_at(i,
                        core::Field(pipelines::pipeline_status_name(entries[i].status)));
                    owner_col->insert_at(i, core::Field(entries[i].owner));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("status", status_col);
                block->add_column("owner", owner_col);
                result_.block = block;
            } else if (node->show_type == "STAGES") {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto* pipeline = mgr.get_pipeline(node->table_name);
                if (!pipeline) {
                    throw common::Exception{
                        "Unknown pipeline: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto name_col = std::make_shared<columns::ColumnString>();
                auto order_col = std::make_shared<columns::ColumnString>();
                auto task_count_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < pipeline->stages.size(); ++i) {
                    name_col->insert_at(i, core::Field(pipeline->stages[i].name));
                    order_col->insert_at(i,
                        core::Field(std::to_string(pipeline->stages[i].order)));
                    task_count_col->insert_at(i,
                        core::Field(std::to_string(pipeline->stages[i].tasks.size())));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("stage_order", order_col);
                block->add_column("task_count", task_count_col);
                result_.block = block;
            } else if (node->show_type == "TASKS") {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto* pipeline = mgr.get_pipeline(node->table_name);
                if (!pipeline) {
                    throw common::Exception{
                        "Unknown pipeline: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto name_col = std::make_shared<columns::ColumnString>();
                auto stage_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto deps_col = std::make_shared<columns::ColumnString>();
                size_t row = 0;
                for (const auto& stage : pipeline->stages) {
                    for (const auto& task : stage.tasks) {
                        name_col->insert_at(row, core::Field(task.name));
                        stage_col->insert_at(row, core::Field(task.stage_name));
                        type_col->insert_at(row, core::Field(pipelines::task_type_name(task.type)));
                        std::string deps;
                        for (size_t j = 0; j < task.depends_on.size(); ++j) {
                            if (j > 0) deps += ",";
                            deps += task.depends_on[j];
                        }
                        deps_col->insert_at(row, core::Field(deps));
                        ++row;
                    }
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("stage", stage_col);
                block->add_column("type", type_col);
                block->add_column("depends_on", deps_col);
                result_.block = block;
            } else if (node->show_type == "TRIGGERS") {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto* pipeline = mgr.get_pipeline(node->table_name);
                if (!pipeline) {
                    throw common::Exception{
                        "Unknown pipeline: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto schedule_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < pipeline->triggers.size(); ++i) {
                    name_col->insert_at(i, core::Field(pipeline->triggers[i].name));
                    type_col->insert_at(i,
                        core::Field(pipelines::trigger_type_name(pipeline->triggers[i].type)));
                    schedule_col->insert_at(i, core::Field(pipeline->triggers[i].schedule));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("schedule", schedule_col);
                result_.block = block;
            } else if (node->show_type == "PIPELINE_RUNS") {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto runs = mgr.list_runs(node->table_name);
                auto run_col = std::make_shared<columns::ColumnString>();
                auto pipeline_col = std::make_shared<columns::ColumnString>();
                auto trigger_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto duration_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < runs.size(); ++i) {
                    run_col->insert_at(i, core::Field(runs[i].run_id));
                    pipeline_col->insert_at(i, core::Field(runs[i].pipeline_name));
                    trigger_col->insert_at(i, core::Field(runs[i].trigger_name));
                    status_col->insert_at(i,
                        core::Field(pipelines::run_status_name(runs[i].status)));
                    duration_col->insert_at(i,
                        core::Field(std::to_string(runs[i].duration_ms)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("run_id", run_col);
                block->add_column("pipeline", pipeline_col);
                block->add_column("trigger", trigger_col);
                block->add_column("status", status_col);
                block->add_column("duration_ms", duration_col);
                result_.block = block;
            } else if (node->show_type == "PIPELINE_METRICS") {
                auto& mgr = pipelines::PipelineManager::instance();
                std::vector<pipelines::PipelineEntry> pipelines_list;
                if (node->table_name.empty()) {
                    pipelines_list = mgr.list_pipelines();
                } else {
                    const auto* entry = mgr.get_pipeline(node->table_name);
                    if (!entry) {
                        throw common::Exception{
                            "Unknown pipeline: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                    }
                    pipelines_list.push_back(*entry);
                }
                auto name_col = std::make_shared<columns::ColumnString>();
                auto runs_col = std::make_shared<columns::ColumnString>();
                auto success_col = std::make_shared<columns::ColumnString>();
                auto avg_duration_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < pipelines_list.size(); ++i) {
                    const auto& p = pipelines_list[i];
                    size_t succeeded = 0;
                    double total_ms = 0.0;
                    for (const auto& run : p.runs) {
                        if (run.status == pipelines::RunStatus::Succeeded) {
                            ++succeeded;
                        }
                        total_ms += run.duration_ms;
                    }
                    const size_t total_runs = p.runs.size();
                    const double success_rate = total_runs > 0
                        ? (100.0 * static_cast<double>(succeeded)
                           / static_cast<double>(total_runs))
                        : 0.0;
                    const double avg_ms = total_runs > 0 ? total_ms / static_cast<double>(total_runs)
                                                         : 0.0;
                    name_col->insert_at(i, core::Field(p.name));
                    runs_col->insert_at(i, core::Field(std::to_string(total_runs)));
                    success_col->insert_at(i, core::Field(std::to_string(success_rate)));
                    avg_duration_col->insert_at(i, core::Field(std::to_string(avg_ms)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("pipeline", name_col);
                block->add_column("run_count", runs_col);
                block->add_column("success_rate_pct", success_col);
                block->add_column("avg_duration_ms", avg_duration_col);
                result_.block = block;
            } else if (node->show_type == "STREAMS") {
                auto& mgr = streaming::StreamManager::instance();
                const auto entries = mgr.list_streams();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto topic_col = std::make_shared<columns::ColumnString>();
                auto events_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    status_col->insert_at(i,
                        core::Field(streaming::stream_status_name(entries[i].status)));
                    topic_col->insert_at(i, core::Field(entries[i].topic_name));
                    events_col->insert_at(i,
                        core::Field(std::to_string(mgr.event_count(entries[i].name))));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("status", status_col);
                block->add_column("topic", topic_col);
                block->add_column("event_count", events_col);
                result_.block = block;
            } else if (node->show_type == "TOPICS") {
                auto& mgr = streaming::StreamManager::instance();
                const auto entries = mgr.list_topics();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto partitions_col = std::make_shared<columns::ColumnString>();
                auto retention_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    partitions_col->insert_at(i,
                        core::Field(std::to_string(entries[i].partition_count)));
                    const std::string retention = entries[i].retention == streaming::RetentionPolicy::Forever
                        ? "FOREVER"
                        : std::to_string(entries[i].retention_days) + " DAYS";
                    retention_col->insert_at(i, core::Field(retention));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("partition_count", partitions_col);
                block->add_column("retention", retention_col);
                result_.block = block;
            } else if (node->show_type == "CONSUMER_GROUPS") {
                auto& mgr = streaming::StreamManager::instance();
                const auto entries = mgr.list_consumer_groups();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto streams_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    std::string tracked;
                    for (const auto& [stream_name, offset] : entries[i].offsets) {
                        if (!tracked.empty()) tracked += ",";
                        tracked += stream_name + ":" + std::to_string(offset);
                    }
                    streams_col->insert_at(i, core::Field(tracked));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("offsets", streams_col);
                result_.block = block;
            } else if (node->show_type == "STREAM_METRICS") {
                auto& mgr = streaming::StreamManager::instance();
                std::vector<std::string> stream_names;
                if (node->table_name.empty()) {
                    for (const auto& entry : mgr.list_streams()) {
                        stream_names.push_back(entry.name);
                    }
                } else {
                    stream_names.push_back(node->table_name);
                }
                auto stream_col = std::make_shared<columns::ColumnString>();
                auto events_col = std::make_shared<columns::ColumnString>();
                auto lag_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < stream_names.size(); ++i) {
                    const auto metrics = mgr.stream_metrics(stream_names[i], "");
                    stream_col->insert_at(i, core::Field(metrics.stream_name));
                    events_col->insert_at(i, core::Field(std::to_string(metrics.event_count)));
                    lag_col->insert_at(i, core::Field(std::to_string(metrics.consumer_lag)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("stream", stream_col);
                block->add_column("event_count", events_col);
                block->add_column("consumer_lag", lag_col);
                result_.block = block;
            } else if (node->show_type == "REPLICATION_STATUS") {
                auto& mgr = replica_groups::ReplicaGroupManager::instance();
                mgr.refresh_lag();
                const auto members = mgr.list_all_members();
                auto group_col = std::make_shared<columns::ColumnString>();
                auto replica_col = std::make_shared<columns::ColumnString>();
                auto node_col = std::make_shared<columns::ColumnString>();
                auto role_col = std::make_shared<columns::ColumnString>();
                auto state_col = std::make_shared<columns::ColumnString>();
                auto lag_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < members.size(); ++i) {
                    group_col->insert_at(i, core::Field(members[i].group_name));
                    replica_col->insert_at(i, core::Field(members[i].replica_id));
                    node_col->insert_at(i, core::Field(members[i].node_id));
                    role_col->insert_at(i, core::Field(replica_groups::replica_role_name(members[i].role)));
                    state_col->insert_at(i, core::Field(replica_groups::replica_state_name(members[i].state)));
                    lag_col->insert_at(i, core::Field(std::to_string(members[i].lag_ms)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("replica_group", group_col);
                block->add_column("replica_id", replica_col);
                block->add_column("node_id", node_col);
                block->add_column("role", role_col);
                block->add_column("state", state_col);
                block->add_column("lag_ms", lag_col);
                result_.block = block;
            } else if (node->show_type == "MODELS") {
                const auto entries = models::ModelManager::instance().list_models();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto type_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                auto ver_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    type_col->insert_at(i, core::Field(std::string(models::model_type_name(entries[i].type))));
                    status_col->insert_at(i, core::Field(entries[i].status));
                    ver_col->insert_at(i, core::Field(std::to_string(entries[i].latest_version)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("type", type_col);
                block->add_column("status", status_col);
                block->add_column("latest_version", ver_col);
                result_.block = block;
            } else if (node->show_type == "MODEL_VERSIONS") {
                const auto entries = models::ModelManager::instance().list_versions(node->table_name);
                auto model_col = std::make_shared<columns::ColumnString>();
                auto ver_col = std::make_shared<columns::ColumnString>();
                auto run_col = std::make_shared<columns::ColumnString>();
                auto artifact_col = std::make_shared<columns::ColumnString>();
                auto metrics_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    model_col->insert_at(i, core::Field(entries[i].model));
                    ver_col->insert_at(i, core::Field("v" + std::to_string(entries[i].version)));
                    run_col->insert_at(i, core::Field(entries[i].run_id));
                    artifact_col->insert_at(i, core::Field(entries[i].artifact_location));
                    std::string m;
                    for (const auto& [k, v] : entries[i].metrics) {
                        if (!m.empty()) m += ", ";
                        m += k + "=" + std::to_string(v);
                    }
                    metrics_col->insert_at(i, core::Field(m));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("model", model_col);
                block->add_column("version", ver_col);
                block->add_column("run_id", run_col);
                block->add_column("artifact", artifact_col);
                block->add_column("metrics", metrics_col);
                result_.block = block;
            } else if (node->show_type == "MODEL_ENDPOINTS") {
                const auto entries = models::ModelManager::instance().list_endpoints();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto model_col = std::make_shared<columns::ColumnString>();
                auto ver_col = std::make_shared<columns::ColumnString>();
                auto status_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (!node->table_name.empty() && entries[i].model != node->table_name) continue;
                    name_col->insert(core::Field(entries[i].name));
                    model_col->insert(core::Field(entries[i].model));
                    ver_col->insert(core::Field("v" + std::to_string(entries[i].version)));
                    status_col->insert(core::Field(std::string(models::endpoint_status_name(entries[i].status))));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("endpoint", name_col);
                block->add_column("model", model_col);
                block->add_column("version", ver_col);
                block->add_column("status", status_col);
                result_.block = block;
            } else if (node->show_type == "MODEL_METRICS") {
                const auto endpoints = models::ModelManager::instance().list_endpoints();
                auto model_col = std::make_shared<columns::ColumnString>();
                auto ep_col = std::make_shared<columns::ColumnString>();
                auto count_col = std::make_shared<columns::ColumnString>();
                auto fail_col = std::make_shared<columns::ColumnString>();
                auto lat_col = std::make_shared<columns::ColumnString>();
                for (const auto& ep : endpoints) {
                    if (!node->table_name.empty() && ep.model != node->table_name) continue;
                    model_col->insert(core::Field(ep.model));
                    ep_col->insert(core::Field(ep.name));
                    count_col->insert(core::Field(std::to_string(ep.prediction_count)));
                    fail_col->insert(core::Field(std::to_string(ep.failure_count)));
                    const double avg = ep.prediction_count > 0
                        ? ep.total_latency_ms / static_cast<double>(ep.prediction_count) : 0.0;
                    lat_col->insert(core::Field(std::to_string(avg)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("model", model_col);
                block->add_column("endpoint", ep_col);
                block->add_column("prediction_count", count_col);
                block->add_column("failure_count", fail_col);
                block->add_column("avg_latency_ms", lat_col);
                result_.block = block;
            } else if (node->show_type == "MODEL_DRIFT") {
                auto model_col = std::make_shared<columns::ColumnString>();
                auto note_col = std::make_shared<columns::ColumnString>();
                model_col->insert(core::Field(node->table_name));
                note_col->insert(core::Field(std::string(
                    "drift monitoring records prediction volume/failure counts per endpoint; "
                    "see SHOW MODEL METRICS")));
                auto block = std::make_shared<core::Block>();
                block->add_column("model", model_col);
                block->add_column("note", note_col);
                result_.block = block;
            } else if (node->show_type == "FEATURE_SETS") {
                const auto entries = feature_sets::FeatureSetManager::instance().list_feature_sets();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto src_col = std::make_shared<columns::ColumnString>();
                auto key_col = std::make_shared<columns::ColumnString>();
                auto feat_col = std::make_shared<columns::ColumnString>();
                auto target_col = std::make_shared<columns::ColumnString>();
                auto ver_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    src_col->insert_at(i, core::Field(entries[i].source_table));
                    key_col->insert_at(i, core::Field(entries[i].entity_key));
                    std::string feats;
                    for (const auto& f : entries[i].features) {
                        if (!feats.empty()) feats += ",";
                        feats += f;
                    }
                    feat_col->insert_at(i, core::Field(feats));
                    target_col->insert_at(i, core::Field(entries[i].target));
                    ver_col->insert_at(i, core::Field(std::to_string(entries[i].version)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("source_table", src_col);
                block->add_column("entity_key", key_col);
                block->add_column("features", feat_col);
                block->add_column("target", target_col);
                block->add_column("version", ver_col);
                result_.block = block;
            } else if (node->show_type == "DATASETS") {
                const auto entries = feature_sets::FeatureSetManager::instance().list_datasets();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto src_col = std::make_shared<columns::ColumnString>();
                auto ver_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    src_col->insert_at(i, core::Field(entries[i].source));
                    ver_col->insert_at(i, core::Field(std::to_string(entries[i].version)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("source", src_col);
                block->add_column("version", ver_col);
                result_.block = block;
            } else if (node->show_type == "TRAINING_JOBS") {
                const auto entries = models::ModelManager::instance().list_training_jobs();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto model_col = std::make_shared<columns::ColumnString>();
                auto fs_col = std::make_shared<columns::ColumnString>();
                auto fw_col = std::make_shared<columns::ColumnString>();
                auto algo_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    model_col->insert_at(i, core::Field(entries[i].model));
                    fs_col->insert_at(i, core::Field(entries[i].feature_set));
                    fw_col->insert_at(i, core::Field(std::string(models::framework_name(entries[i].framework))));
                    algo_col->insert_at(i, core::Field(entries[i].algorithm));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("model", model_col);
                block->add_column("feature_set", fs_col);
                block->add_column("framework", fw_col);
                block->add_column("algorithm", algo_col);
                result_.block = block;
            } else if (node->show_type == "TUNING_JOBS") {
                const auto entries = models::ModelManager::instance().list_tuning_jobs();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto model_col = std::make_shared<columns::ColumnString>();
                auto strat_col = std::make_shared<columns::ColumnString>();
                auto obj_col = std::make_shared<columns::ColumnString>();
                auto trials_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    model_col->insert_at(i, core::Field(entries[i].model));
                    strat_col->insert_at(i, core::Field(std::string(models::tuning_strategy_name(entries[i].strategy))));
                    obj_col->insert_at(i, core::Field(entries[i].objective));
                    trials_col->insert_at(i, core::Field(std::to_string(entries[i].trials)));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("model", model_col);
                block->add_column("strategy", strat_col);
                block->add_column("objective", obj_col);
                block->add_column("trials", trials_col);
                result_.block = block;
            } else if (node->show_type == "MODEL_TEMPLATES") {
                const auto entries = models::ModelManager::instance().list_templates();
                auto name_col = std::make_shared<columns::ColumnString>();
                auto fw_col = std::make_shared<columns::ColumnString>();
                auto algo_col = std::make_shared<columns::ColumnString>();
                for (size_t i = 0; i < entries.size(); ++i) {
                    name_col->insert_at(i, core::Field(entries[i].name));
                    fw_col->insert_at(i, core::Field(std::string(models::framework_name(entries[i].framework))));
                    algo_col->insert_at(i, core::Field(entries[i].algorithm));
                }
                auto block = std::make_shared<core::Block>();
                block->add_column("name", name_col);
                block->add_column("framework", fw_col);
                block->add_column("algorithm", algo_col);
                result_.block = block;
            }
            break;
        }
        case planner::PlanNode::Type::DESCRIBE: {
            std::shared_ptr<storages::IStorage> storage;
            {
                using OK = parsers::QueryAST::ObjectKind;
                const auto kind = node->describe_object_kind;
                if (kind == OK::Model || kind == OK::ModelVersion || kind == OK::TrainingJob ||
                    kind == OK::TuningJob || kind == OK::ModelTemplate || kind == OK::FeatureSet ||
                    kind == OK::Dataset) {
                    auto field_col = std::make_shared<columns::ColumnString>();
                    auto value_col = std::make_shared<columns::ColumnString>();
                    auto add_row = [&](const std::string& f, const std::string& v) {
                        field_col->insert(core::Field(f));
                        value_col->insert(core::Field(v));
                    };
                    auto join = [](const std::vector<std::string>& xs) {
                        std::string s;
                        for (const auto& x : xs) { if (!s.empty()) s += ","; s += x; }
                        return s;
                    };
                    auto& mm = models::ModelManager::instance();
                    auto& fm = feature_sets::FeatureSetManager::instance();
                    if (kind == OK::Model) {
                        auto e = mm.get_model(node->table_name);
                        if (!e) throw common::Exception{"Unknown model: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("type", std::string(models::model_type_name(e->type)));
                        add_row("status", e->status);
                        add_row("latest_version", std::to_string(e->latest_version));
                        add_row("owner", e->owner);
                    } else if (kind == OK::ModelVersion) {
                        auto e = mm.get_version(node->table_name, node->model_version);
                        if (!e) throw common::Exception{"Unknown model version: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("model", e->model);
                        add_row("version", "v" + std::to_string(e->version));
                        add_row("run_id", e->run_id);
                        add_row("artifact", e->artifact_location);
                        for (const auto& [k, v] : e->metrics) add_row(k, std::to_string(v));
                    } else if (kind == OK::TrainingJob) {
                        auto e = mm.get_training_job(node->table_name);
                        if (!e) throw common::Exception{"Unknown training job: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("model", e->model);
                        add_row("feature_set", e->feature_set);
                        add_row("dataset", e->dataset);
                        add_row("framework", std::string(models::framework_name(e->framework)));
                        add_row("algorithm", e->algorithm);
                        add_row("objective", e->objective);
                        for (const auto& [k, v] : e->hyperparams) add_row("hp." + k, v);
                    } else if (kind == OK::TuningJob) {
                        auto e = mm.get_tuning_job(node->table_name);
                        if (!e) throw common::Exception{"Unknown tuning job: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("model", e->model);
                        add_row("training_job", e->training_job);
                        add_row("strategy", std::string(models::tuning_strategy_name(e->strategy)));
                        add_row("objective", e->objective);
                        add_row("trials", std::to_string(e->trials));
                        for (const auto& [k, v] : e->search_space) add_row("space." + k, v);
                    } else if (kind == OK::ModelTemplate) {
                        auto e = mm.get_template(node->table_name);
                        if (!e) throw common::Exception{"Unknown model template: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("framework", std::string(models::framework_name(e->framework)));
                        add_row("algorithm", e->algorithm);
                    } else if (kind == OK::FeatureSet) {
                        const auto* e = fm.get_feature_set(node->table_name);
                        if (!e) throw common::Exception{"Unknown feature set: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("source_table", e->source_table);
                        add_row("entity_key", e->entity_key);
                        add_row("features", join(e->features));
                        add_row("target", e->target);
                        add_row("version", std::to_string(e->version));
                    } else { // Dataset
                        const auto* e = fm.get_dataset(node->table_name);
                        if (!e) throw common::Exception{"Unknown dataset: " + node->table_name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                        add_row("name", e->name);
                        add_row("source", e->source);
                        add_row("version", std::to_string(e->version));
                    }
                    auto out = std::make_shared<core::Block>();
                    out->add_column("field", field_col);
                    out->add_column("value", value_col);
                    result_.block = out;
                    break;
                }
            }
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
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::ReplicaGroup) {
                auto& mgr = replica_groups::ReplicaGroupManager::instance();
                mgr.refresh_lag();
                const auto* entry = mgr.get_group(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown replica group: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("replication_factor", std::to_string(entry->replication_factor));
                add_row("strategy", replica_groups::replica_strategy_name(entry->strategy));
                add_row("consistency", replica_groups::consistency_mode_name(entry->consistency_mode));
                add_row("placement", replica_groups::placement_policy_name(entry->placement_policy));
                add_row("status", replica_groups::replica_group_status_name(entry->status));
                add_row("reference_count", std::to_string(entry->reference_count));
                const auto events = mgr.list_failover_events();
                add_row("failover_events", std::to_string(events.size()));
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::ShardGroup) {
                auto& mgr = shard_groups::ShardGroupManager::instance();
                const auto* entry = mgr.get_group(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown shard group: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("type", shard_groups::shard_strategy_name(entry->strategy));
                add_row("shard_count", std::to_string(entry->shard_count));
                add_row("shard_key", entry->shard_key);
                add_row("status", shard_groups::shard_group_status_name(entry->status));
                add_row("reference_count", std::to_string(entry->reference_count));
                const auto members = mgr.list_members(entry->name);
                add_row("active_shards", std::to_string(members.size()));
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Stream) {
                auto& mgr = streaming::StreamManager::instance();
                const auto* entry = mgr.get_stream(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown stream: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("status", streaming::stream_status_name(entry->status));
                add_row("topic", entry->topic_name);
                add_row("retention", entry->retention == streaming::RetentionPolicy::Forever
                    ? "FOREVER"
                    : std::to_string(entry->retention_days) + " DAYS");
                add_row("event_count", std::to_string(mgr.event_count(entry->name)));
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Topic) {
                auto& mgr = streaming::StreamManager::instance();
                const auto* entry = mgr.get_topic(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown topic: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("partition_count", std::to_string(entry->partition_count));
                add_row("retention", entry->retention == streaming::RetentionPolicy::Forever
                    ? "FOREVER"
                    : std::to_string(entry->retention_days) + " DAYS");
                const auto bound = mgr.streams_bound_to_topic(entry->name);
                std::string streams;
                for (size_t i = 0; i < bound.size(); ++i) {
                    if (i > 0) streams += ",";
                    streams += bound[i];
                }
                add_row("bound_streams", streams);
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::ConsumerGroup) {
                auto& mgr = streaming::StreamManager::instance();
                const auto* entry = mgr.get_consumer_group(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown consumer group: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                for (const auto& [stream_name, offset] : entry->offsets) {
                    add_row("offset_" + stream_name, std::to_string(offset));
                }
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Pipeline) {
                auto& mgr = pipelines::PipelineManager::instance();
                const auto* entry = mgr.get_pipeline(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown pipeline: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("status", pipelines::pipeline_status_name(entry->status));
                add_row("owner", entry->owner);
                add_row("stage_count", std::to_string(entry->stages.size()));
                add_row("trigger_count", std::to_string(entry->triggers.size()));
                add_row("run_count", std::to_string(entry->runs.size()));
                auto out = std::make_shared<core::Block>();
                out->add_column("field", field_col);
                out->add_column("value", value_col);
                result_.block = out;
                break;
            }
            if (node->describe_object_kind == parsers::QueryAST::ObjectKind::Connector) {
                auto& mgr = connectors::ConnectorManager::instance();
                const auto* entry = mgr.get_connector(node->table_name);
                if (!entry) {
                    throw common::Exception{
                        "Unknown connector: " + node->table_name,
                        static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
                }
                auto field_col = std::make_shared<columns::ColumnString>();
                auto value_col = std::make_shared<columns::ColumnString>();
                auto add_row = [&](const std::string& field, const std::string& value) {
                    field_col->insert(core::Field(field));
                    value_col->insert(core::Field(value));
                };
                add_row("name", entry->name);
                add_row("type", connectors::connector_type_name(entry->type));
                add_row("status", connectors::connector_status_name(entry->status));
                add_row("auth", connectors::auth_method_name(entry->auth));
                for (const auto& [key, value] : entry->properties) {
                    add_row(key, connectors::mask_property_value(key, value));
                }
                std::string caps;
                for (size_t i = 0; i < entry->capabilities.size(); ++i) {
                    if (i > 0) caps += ",";
                    caps += entry->capabilities[i];
                }
                add_row("capabilities", caps);
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
