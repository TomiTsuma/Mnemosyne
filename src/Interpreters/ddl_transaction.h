// src/Interpreters/ddl_transaction.h — Lightweight DDL rollback on failure
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Interpreters/context.h"
#include "Storages/i_storage.h"
#include "Storages/table_metadata.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace mnemo::interpreters {

// ── DDLTransaction — snapshots metadata and restores it on exception ──
class DDLTransaction {
public:
    explicit DDLTransaction(Context& context);
    ~DDLTransaction();

    void snapshot_table(std::string database, std::string table);
    void commit();

    DDLTransaction(const DDLTransaction&)            = delete;
    DDLTransaction& operator=(const DDLTransaction&) = delete;

private:
    struct TableSnapshot {
        bool existed = false;
        std::shared_ptr<storages::IStorage> storage;
        storages::TableMetadata metadata;
    };

    void rollback();

    Context& context_;
    bool committed_ = false;
    std::unordered_map<std::string, TableSnapshot> snapshots_;
};

} // namespace mnemo::interpreters
