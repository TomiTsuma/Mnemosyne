// src/Interpreters/interpreter_create_query.cpp — CREATE interpreter implementation

#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Storages/storage_factory.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterCreateQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    DDLTransaction txn{context};

    if (!query.create.table_name.empty()) {
        const auto db_name = ddl_utils::resolve_current_database(context);
        txn.snapshot_table(db_name, query.create.table_name);
        do_create_table(context, query.create);
    } else if (!query.create.database_name.empty()) {
        do_create_database(context, query.create);
    } else {
        throw common::Exception{
            "CREATE: expected DATABASE or TABLE",
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
    if (db->table_exists(create.table_name)) {
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
    if (storage) {
        context.register_storage(create.table_name, storage);
    }
}

} // namespace mnemo::interpreters
