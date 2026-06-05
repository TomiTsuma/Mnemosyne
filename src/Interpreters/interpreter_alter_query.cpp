// src/Interpreters/interpreter_alter_query.cpp — ALTER TABLE interpreter implementation

#include "Interpreters/interpreter_alter_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Storages/memory_storage.h"
#include "DataTypes/data_type_factory.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterAlterQuery::add_column(storages::IStorage& storage,
                                       const std::string& name,
                                       const std::string& type_name) -> void {
    auto type = datatypes::get_data_type(type_name);
    if (auto mem = dynamic_cast<storages::MemoryStorage*>(&storage)) {
        mem->add_column(name, type);
        return;
    }
    storage.alter([&](storages::IStorage& s) {
        if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
            mem->add_column(name, type);
        }
    });
}

auto InterpreterAlterQuery::drop_column(storages::IStorage& storage, const std::string& name)
    -> void {
    if (auto mem = dynamic_cast<storages::MemoryStorage*>(&storage)) {
        mem->drop_column(name);
        return;
    }
    storage.alter([&](storages::IStorage& s) {
        if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
            mem->drop_column(name);
        }
    });
}

auto InterpreterAlterQuery::modify_column(storages::IStorage& storage,
                                          const std::string& name,
                                          const std::string& type_name) -> void {
    auto type = datatypes::get_data_type(type_name);
    if (auto mem = dynamic_cast<storages::MemoryStorage*>(&storage)) {
        mem->modify_column(name, type);
        return;
    }
    storage.alter([&](storages::IStorage& s) {
        if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
            mem->modify_column(name, type);
        }
    });
}

auto InterpreterAlterQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLTransaction txn{context};
    txn.snapshot_table(db_name, query.alter.table);
    DDLGuard guard{db_name, query.alter.table};

    auto db = ddl_utils::require_database(context, db_name);
    if (!db->table_exists(query.alter.table)) {
        throw common::Exception{
            "Unknown table: " + query.alter.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto storage = db->table(query.alter.table);
    if (!storage) {
        throw common::Exception{
            "Unknown table: " + query.alter.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    for (const auto& cmd : query.alter.commands) {
        switch (cmd.type) {
            case parsers::ASTAlterQuery::AlterCommand::Type::ADD_COLUMN:
                add_column(*storage, cmd.column_name, cmd.column_type);
                break;
            case parsers::ASTAlterQuery::AlterCommand::Type::DROP_COLUMN:
                drop_column(*storage, cmd.column_name);
                break;
            case parsers::ASTAlterQuery::AlterCommand::Type::MODIFY_COLUMN:
                modify_column(*storage, cmd.column_name, cmd.column_type);
                break;
        }
    }

    txn.commit();
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
