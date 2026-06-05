// src/Databases/database_manager.h — DatabaseManager: registry of databases
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "database.h"
#include "i_database.h"
#include "Storages/i_storage.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::databases {

class DatabaseManager {
public:
    static auto instance() -> DatabaseManager&;

    auto create_database(std::string name) -> std::shared_ptr<Database>;
    auto get_database(std::string_view name) -> std::shared_ptr<Database>;
    auto get_database(std::string_view name) const -> std::shared_ptr<Database>;

    void register_idatabase(std::string name, std::shared_ptr<IDatabase> db);
    auto get_idatabase(std::string_view name) -> std::shared_ptr<IDatabase>;
    auto get_idatabase(std::string_view name) const -> std::shared_ptr<IDatabase>;

    void drop_database(std::string name);

    [[nodiscard]] auto database_names() const -> std::vector<std::string>;
    [[nodiscard]] auto database_count() const -> size_t;
    [[nodiscard]] auto has_database(std::string_view name) const -> bool;

    [[nodiscard]] auto list_tables(std::string_view database) const -> std::vector<std::string>;
    [[nodiscard]] auto get_table_storage(std::string_view database, std::string_view table)
        -> std::shared_ptr<storages::IStorage>;

private:
    DatabaseManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Database>>   databases_;
    std::unordered_map<std::string, std::shared_ptr<IDatabase>> idatabases_;
};

} // namespace mnemo::databases
