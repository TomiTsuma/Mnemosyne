// src/Databases/database_memory.h — Memory database engine
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_database.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace mnesso::databases {

// ── DatabaseMemory — stores tables in memory ──
class DatabaseMemory final : public IDatabase {
public:
    static auto create(std::string name, std::string path = "")
        -> std::shared_ptr<DatabaseMemory>;

    // IDatabase interface
    [[nodiscard]] auto name()      const -> std::string override;
    [[nodiscard]] auto path()      const -> std::string override;
    [[nodiscard]] auto engine()    const -> std::string override;
    [[nodiscard]] auto tables()    const -> std::vector<std::string> override;
    auto attach_table(std::string name, std::shared_ptr<storages::IStorage> table) -> void override;
    auto detach_table(std::string name) -> std::shared_ptr<storages::IStorage> override;
    auto rename_table(std::string from, std::string to) -> bool override;
    [[nodiscard]] auto table_exists(std::string_view name) const -> bool override;
    [[nodiscard]] auto table(std::string name) -> std::shared_ptr<storages::IStorage> override;
    auto drop_table(std::string name) -> bool override;
    auto create_table(std::string name,
                      std::unordered_map<std::string, datatypes::DataTypePtr> columns,
                      std::string engine = "Memory") -> std::shared_ptr<storages::IStorage> override;

    // Memory-specific
    auto clear_all() -> void;

private:
    DatabaseMemory();
    std::string                    name_;
    std::string                    path_;
    std::unordered_map<std::string, std::shared_ptr<storages::IStorage>> tables_;
    mutable std::mutex             mutex_;
};

} // namespace mnesso::databases
