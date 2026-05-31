// src/Databases/i_database.h — IDatabase: abstract database interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "DataTypes/data_type.h"
#include "Storages/i_storage.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mnesso::databases {

// ── IDatabase — catalog of tables backed by storage engines ──
class IDatabase {
public:
    virtual ~IDatabase() = default;

    [[nodiscard]] virtual auto name()   const -> std::string = 0;
    [[nodiscard]] virtual auto path()   const -> std::string = 0;
    [[nodiscard]] virtual auto engine() const -> std::string = 0;

    [[nodiscard]] virtual auto tables() const -> std::vector<std::string> = 0;

    virtual auto attach_table(std::string name, std::shared_ptr<storages::IStorage> table) -> void = 0;
    virtual auto detach_table(std::string name) -> std::shared_ptr<storages::IStorage> = 0;
    virtual auto rename_table(std::string from, std::string to) -> bool = 0;
    [[nodiscard]] virtual auto table_exists(std::string_view name) const -> bool = 0;
    [[nodiscard]] virtual auto table(std::string name) -> std::shared_ptr<storages::IStorage> = 0;
    virtual auto drop_table(std::string name) -> bool = 0;

    virtual auto create_table(
        std::string name,
        std::unordered_map<std::string, datatypes::DataTypePtr> columns,
        std::string engine) -> std::shared_ptr<storages::IStorage> = 0;
};

} // namespace mnesso::databases
