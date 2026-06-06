// src/Interpreters/interpreter_create_query.cpp — CREATE interpreter implementation

#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_select_query.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Databases/view_catalog.h"
#include "Storages/memory_storage.h"
#include "Storages/file_storage.h"
#include "Storages/storage_factory.h"
#include "StorageUnits/storage_unit_catalog.h"
#include "StorageUnits/storage_unit_manager.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterCreateQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    DDLTransaction txn{context};

    switch (query.create.kind) {
        case parsers::QueryAST::Create::Kind::View:
            do_create_view(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::MaterializedView: {
            const auto db_name = ddl_utils::resolve_current_database(context);
            txn.snapshot_table(db_name, query.create.view_name);
            do_create_materialized_view(context, query.create);
            break;
        }
        case parsers::QueryAST::Create::Kind::Table: {
            const auto db_name = ddl_utils::resolve_current_database(context);
            txn.snapshot_table(db_name, query.create.table_name);
            do_create_table(context, query.create);
            break;
        }
        case parsers::QueryAST::Create::Kind::Database:
            do_create_database(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::StorageUnit:
            do_create_storage_unit(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Node:
            do_create_node(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Cluster:
            do_create_cluster(context, query.create);
            break;
        default:
            throw common::Exception{
                "CREATE: expected DATABASE, TABLE, VIEW, MATERIALIZED VIEW, STORAGE_UNIT, NODE, or CLUSTER",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    txn.commit();
    return ddl_utils::make_ok_block();
}

auto InterpreterCreateQuery::do_create_database(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    if (context.get_database(create.database_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "Database already exists: " + create.database_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto& db_manager = databases::DatabaseManager::instance();
    std::shared_ptr<databases::IDatabase> db = db_manager.create_database(create.database_name);
    if (!db) {
        db = databases::DatabaseMemory::create(create.database_name);
    }
    context.register_database(create.database_name, db);
}

auto InterpreterCreateQuery::do_create_table(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.table_name};

    auto db = ddl_utils::require_database(context, db_name);
    const bool exists = [&] {
        if (auto catalog = std::dynamic_pointer_cast<databases::Database>(db)) {
            return catalog->relation_exists(create.table_name);
        }
        return db->table_exists(create.table_name);
    }();
    if (exists) {
        if (create.if_not_exists) {
            if (auto existing = db->table(create.table_name)) {
                context.register_storage(create.table_name, existing);
            }
            return;
        }
        throw common::Exception{
            "Table already exists: " + create.table_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    const auto engine = create.engine.empty() ? "Memory" : create.engine;
    if (!storages::StorageFactory::instance().has(engine)) {
        throw common::Exception{
            "Unknown storage engine: " + engine,
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }

    auto columns = ddl_utils::build_column_map(create.columns);
    auto storage = db->create_table(create.table_name, columns, engine);
    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->set_columns(columns);
    } else if (auto file = std::dynamic_pointer_cast<storages::FileStorage>(storage)) {
        file->set_columns(columns);
    }

    if (!create.storage_unit_name.empty()) {
        auto& mgr = storage_units::StorageUnitManager::instance();
        const auto* unit = mgr.get_unit(create.storage_unit_name);
        if (!unit) {
            throw common::Exception{
                "Unknown storage unit: " + create.storage_unit_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        if (engine != "File") {
            throw common::Exception{
                "STORAGE_UNIT clause requires ENGINE=File",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
        if (unit->type == storage_units::StorageUnitType::S3) {
            throw common::Exception{
                "S3 storage units do not support File engine I/O yet",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
        auto disk = mgr.acquire_disk(create.storage_unit_name);
        if (auto file = std::dynamic_pointer_cast<storages::FileStorage>(storage)) {
            const auto table_path = db_name + "/" + create.table_name;
            file->set_disk(disk);
            file->set_data_path(table_path);
            file->set_storage_unit_name(create.storage_unit_name);
            disk->create_dir(table_path);
        }
    }

    if (storage) {
        context.register_storage(create.table_name, storage);
    }
}

auto InterpreterCreateQuery::do_create_view(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.view_name};

    auto db = ddl_utils::require_catalog(context);
    if (db->relation_exists(create.view_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "View already exists: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto deps = databases::extract_dependencies(create.select_definition);
    for (const auto& dep : deps) {
        if (!db->table_exists(dep) && !db->has_view(dep) && !db->has_materialized_view(dep)) {
            throw common::Exception{
                "Unknown table or view in view definition: " + dep,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
    }

    databases::ViewEntry entry;
    entry.name = create.view_name;
    entry.definition = create.select_definition;
    entry.dependencies = std::move(deps);
    db->create_view(std::move(entry));
}

auto InterpreterCreateQuery::do_create_materialized_view(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.view_name};

    auto db = ddl_utils::require_catalog(context);
    if (db->relation_exists(create.view_name)) {
        if (create.if_not_exists) {
            if (auto storage = db->table(create.view_name)) {
                context.register_storage(create.view_name, storage);
            }
            return;
        }
        throw common::Exception{
            "Materialized view already exists: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto deps = databases::extract_dependencies(create.select_definition);
    for (const auto& dep : deps) {
        if (!db->table_exists(dep) && !db->has_view(dep) && !db->has_materialized_view(dep)) {
            throw common::Exception{
                "Unknown table or view in materialized view definition: " + dep,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
    }

    parsers::QueryAST select_query;
    select_query.query_type = parsers::QueryAST::QueryType::SELECT;
    select_query.select = create.select_definition;
    auto result = InterpreterSelectQuery::execute(context, select_query);

    auto columns = ddl_utils::block_column_map(result);
    if (columns.empty()) {
        throw common::Exception{
            "Materialized view definition produced no columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto storage = db->create_table(create.view_name, columns, "Memory");
    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->load_block(result);
    } else if (!storage->write(result)) {
        db->drop_table(create.view_name);
        throw common::Exception{
            "Failed to populate materialized view: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    databases::MaterializedViewEntry entry;
    entry.name = create.view_name;
    entry.definition = create.select_definition;
    entry.backing_table = create.view_name;
    entry.dependencies = std::move(deps);
    db->create_materialized_view(std::move(entry));
    context.register_storage(create.view_name, storage);
}

auto InterpreterCreateQuery::do_create_storage_unit(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    auto& mgr = storage_units::StorageUnitManager::instance();
    if (mgr.has_unit(create.storage_unit_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "Storage unit already exists: " + create.storage_unit_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    storage_units::StorageUnitEntry entry;
    entry.name = create.storage_unit_name;
    entry.type = storage_units::parse_storage_unit_type(create.storage_unit_type);
    for (const auto& [key, value] : create.storage_properties) {
        if (key == "PATH") {
            entry.path = value;
        } else if (key == "BUCKET") {
            entry.bucket = value;
        } else if (key == "ENDPOINT") {
            entry.endpoint = value;
        } else if (key == "REGION") {
            entry.region = value;
        }
    }
    mgr.create_unit(std::move(entry));
}

auto InterpreterCreateQuery::do_create_node(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    nodes::NodeEntry entry;
    entry.node_name = create.node_name;
    entry.node_id = create.node_name;
    if (!create.node_type.empty()) {
        entry.node_type = nodes::parse_node_type(create.node_type);
    }
    if (!create.node_role.empty()) {
        entry.node_role = nodes::parse_node_role(create.node_role);
    }
    entry.status = nodes::NodeStatus::Registering;
    entry.capabilities = nodes::capabilities_for_type(entry.node_type);
    entry.resources = nodes::detect_local_resources();
    nodes::NodeManager::instance().create_node(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_cluster(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    nodes::ClusterEntry entry;
    entry.name = create.cluster_name;
    nodes::NodeManager::instance().create_cluster(std::move(entry), create.if_not_exists);
}

} // namespace mnemo::interpreters
