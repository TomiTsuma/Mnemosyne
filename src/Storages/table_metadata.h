// src/Storages/table_metadata.h — Versioned table metadata for DDL operations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "DataTypes/data_type.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace mnemo::storages {

// ── TableMetadata — column definitions, engine settings, and schema version ──
class TableMetadata {
public:
    uint64_t version = 1;
    std::string engine = "Memory";
    std::unordered_map<std::string, datatypes::DataTypePtr> columns;

    void increment_version() { ++version; }

    [[nodiscard]] auto clone() const -> TableMetadata;
    [[nodiscard]] auto has_column(std::string_view name) const -> bool;
};

} // namespace mnemo::storages
