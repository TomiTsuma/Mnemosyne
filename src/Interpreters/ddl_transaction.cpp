// src/Interpreters/ddl_transaction.cpp — DDLTransaction implementation

#include "Interpreters/ddl_transaction.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

DDLTransaction::DDLTransaction(Context& context) : context_{context} {}

DDLTransaction::~DDLTransaction() {
    if (!committed_) {
        rollback();
    }
}

void DDLTransaction::snapshot_table(std::string database, std::string table) {
    const auto key = database + "." + table;
    if (snapshots_.contains(key)) {
        return;
    }

    TableSnapshot snapshot;
    auto db = context_.get_database(database);
    if (db && db->table_exists(table)) {
        snapshot.existed  = true;
        snapshot.storage  = db->table(table);
        snapshot.metadata = storages::TableMetadata{};
        snapshot.metadata.engine = snapshot.storage ? snapshot.storage->engine() : "Memory";
        if (snapshot.storage) {
            snapshot.metadata.columns = snapshot.storage->column_types();
        }
    }

    snapshots_.emplace(std::move(key), std::move(snapshot));
}

void DDLTransaction::commit() {
    committed_ = true;
}

void DDLTransaction::rollback() {
    for (auto& [key, snapshot] : snapshots_) {
        const auto dot = key.find('.');
        if (dot == std::string::npos) {
            continue;
        }
        auto database = key.substr(0, dot);
        auto table    = key.substr(dot + 1);

        auto db = context_.get_database(database);
        if (!db) {
            continue;
        }

        if (snapshot.existed) {
            if (snapshot.storage) {
                db->attach_table(table, snapshot.storage);
                context_.register_storage(table, snapshot.storage);
            }
        } else if (db->table_exists(table)) {
            db->drop_table(table);
            context_.unregister_storage(table);
        }
    }
}

} // namespace mnemo::interpreters
