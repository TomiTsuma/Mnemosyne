// src/Interpreters/interpreter_drop_query.cpp — DROP interpreter implementation

#include "Interpreters/interpreter_drop_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Storages/memory_storage.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

namespace {

auto resolve_storage(Context& context, const std::string& table)
    -> std::shared_ptr<storages::IStorage> {
    return ddl_utils::resolve_storage(context, table);
}

} // namespace

auto InterpreterDropQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    DDLTransaction txn{context};
    const auto db_name = ddl_utils::resolve_current_database(context);
    txn.snapshot_table(db_name, query.drop.table);

    switch (query.drop.kind) {
        case parsers::QueryAST::Drop::Kind::Drop:
            if (query.drop.object_kind == parsers::QueryAST::ObjectKind::View) {
                do_drop_view(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::MaterializedView) {
                do_drop_materialized_view(context, query.drop);
            } else {
                do_drop(context, query.drop);
            }
            break;
        case parsers::QueryAST::Drop::Kind::Truncate:
            do_truncate(context, query.drop);
            break;
        case parsers::QueryAST::Drop::Kind::Detach:
            do_detach(context, query.drop);
            break;
    }

    txn.commit();
    return ddl_utils::make_ok_block();
}

auto InterpreterDropQuery::do_drop(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_database(context, db_name);
    if (!db->table_exists(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->drop_table(drop.table);
    context.unregister_storage(drop.table);
}

auto InterpreterDropQuery::do_drop_view(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_catalog(context);
    if (!db->has_view(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown view: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->drop_view(drop.table);
}

auto InterpreterDropQuery::do_drop_materialized_view(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_catalog(context);
    if (!db->has_materialized_view(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown materialized view: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->drop_table(drop.table);
    db->drop_materialized_view(drop.table);
    context.unregister_storage(drop.table);
}

auto InterpreterDropQuery::do_truncate(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto storage = resolve_storage(context, drop.table);
    if (!storage) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->truncate();
        return;
    }

    storage->alter([](storages::IStorage& s) {
        if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
            mem->truncate();
        }
    });
}

auto InterpreterDropQuery::do_detach(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_database(context, db_name);
    if (!db->table_exists(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->detach_table(drop.table);
    context.unregister_storage(drop.table);
}

} // namespace mnemo::interpreters
