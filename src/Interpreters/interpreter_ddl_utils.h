// src/Interpreters/interpreter_ddl_utils.h — Shared helpers for DDL interpreters
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Core/field.h"
#include "DataTypes/data_type_factory.h"
#include "Interpreters/context.h"
#include "Planner/execution_plan.h"
#include "Parsers/ast.h"
#include "Storages/i_storage.h"
#include "Common/exceptions.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace mnemo::interpreters::ddl_utils {

inline auto make_ok_block() -> core::Block {
    core::Block block;
    auto string_type = datatypes::get_data_type("String");
    auto* raw_col    = string_type->create_column();
    auto col         = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col),
        [](void* p) { delete static_cast<core::IColumn*>(p); });
    col->insert(core::Field(std::string{"OK"}));
    block.add_column("result", col);
    return block;
}

inline auto require_database(Context& context, std::string_view name)
    -> std::shared_ptr<databases::IDatabase> {
    auto db = context.get_database(name);
    if (!db) {
        throw common::Exception{
            "Unknown database: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }
    return db;
}

inline auto resolve_current_database(Context& context) -> std::string {
    if (!context.current_database().empty()) {
        return context.current_database();
    }
    return "default";
}

/// Resolve table storage from the current database (source of truth), refreshing context cache.
inline auto resolve_storage(Context& context, const std::string& table)
    -> std::shared_ptr<storages::IStorage> {
    const auto db_name = resolve_current_database(context);
    if (auto db = context.get_database(db_name)) {
        if (auto storage = db->table(table)) {
            context.register_storage(table, storage);
            return storage;
        }
    }
    return context.get_storage(table);
}

inline auto build_column_map(
    const std::vector<parsers::ColumnDef>& columns)
    -> std::unordered_map<std::string, datatypes::DataTypePtr> {
    std::unordered_map<std::string, datatypes::DataTypePtr> result;
    for (const auto& col : columns) {
        result[col.name] = datatypes::get_data_type(col.data_type);
    }
    return result;
}

inline auto query_from_plan(const planner::PlanNode& node) -> parsers::QueryAST {
    parsers::QueryAST query;

    switch (node.node_type) {
        case planner::PlanNode::Type::CREATE:
            query.query_type = parsers::QueryAST::QueryType::CREATE;
            if (!node.table_name.empty()) {
                query.create.table_name = node.table_name;
                query.create.table = node.table_name;
                query.create.columns = node.column_defs;
                query.create.engine = node.engine;
            } else {
                query.create.database_name = node.name;
            }
            query.create.if_not_exists = node.if_not_exists;
            break;
        case planner::PlanNode::Type::INSERT:
            query.query_type = parsers::QueryAST::QueryType::INSERT;
            query.insert.table = node.table;
            query.insert.columns = node.columns;
            query.insert.values = node.values;
            break;
        case planner::PlanNode::Type::DROP:
        case planner::PlanNode::Type::TRUNCATE:
        case planner::PlanNode::Type::DETACH:
            query.query_type = parsers::QueryAST::QueryType::DROP;
            query.drop.table = node.table_name;
            query.drop.if_exists = node.if_exists;
            query.drop.kind = node.drop_kind;
            break;
        case planner::PlanNode::Type::ALTER:
            query.query_type = parsers::QueryAST::QueryType::ALTER;
            query.alter.table = node.table_name;
            query.alter.commands = node.alter_commands;
            break;
        default:
            break;
    }

    return query;
}

} // namespace mnemo::interpreters::ddl_utils
