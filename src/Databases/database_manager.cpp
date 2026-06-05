// src/Databases/database_manager.cpp — Database manager implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "database_manager.h"
#include "database.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::databases {

// ── DatabaseManager ──

auto DatabaseManager::instance() -> DatabaseManager& {
    static DatabaseManager inst;
    return inst;
}

auto DatabaseManager::create_database(std::string name) -> std::shared_ptr<Database> {
    auto db = std::make_shared<Database>(std::move(name));
    databases_[db->name()] = db;
    return db;
}

auto DatabaseManager::get_database(std::string_view name) -> std::shared_ptr<Database> {
    auto it = databases_.find(std::string{name});
    if (it == databases_.end()) return nullptr;
    return it->second;
}

auto DatabaseManager::get_database(std::string_view name) const -> std::shared_ptr<Database> {
    auto it = databases_.find(std::string{name});
    if (it == databases_.end()) return nullptr;
    return it->second;
}

void DatabaseManager::register_idatabase(std::string name, std::shared_ptr<IDatabase> db) {
    idatabases_[std::move(name)] = std::move(db);
}

auto DatabaseManager::get_idatabase(std::string_view name) -> std::shared_ptr<IDatabase> {
    auto it = idatabases_.find(std::string{name});
    if (it == idatabases_.end()) {
        return nullptr;
    }
    return it->second;
}

auto DatabaseManager::get_idatabase(std::string_view name) const -> std::shared_ptr<IDatabase> {
    auto it = idatabases_.find(std::string{name});
    if (it == idatabases_.end()) {
        return nullptr;
    }
    return it->second;
}

void DatabaseManager::drop_database(std::string name) {
    databases_.erase(name);
    idatabases_.erase(name);
}

auto DatabaseManager::database_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(databases_.size() + idatabases_.size());
    for (const auto& [db_name, _] : databases_) {
        names.push_back(db_name);
    }
    for (const auto& [db_name, _] : idatabases_) {
        if (databases_.find(db_name) == databases_.end()) {
            names.push_back(db_name);
        }
    }
    return names;
}

size_t DatabaseManager::database_count() const {
    return database_names().size();
}

bool DatabaseManager::has_database(std::string_view name) const {
    const auto key = std::string{name};
    return databases_.find(key) != databases_.end()
        || idatabases_.find(key) != idatabases_.end();
}

auto DatabaseManager::list_tables(std::string_view database) const -> std::vector<std::string> {
    if (auto catalog = get_database(database)) {
        return catalog->table_names();
    }
    if (auto idb = get_idatabase(database)) {
        return idb->tables();
    }
    return {};
}

auto DatabaseManager::get_table_storage(std::string_view database, std::string_view table)
    -> std::shared_ptr<storages::IStorage> {
    if (auto catalog = get_database(database)) {
        if (auto tbl = catalog->get_table(table)) {
            return tbl->storage();
        }
    }
    if (auto idb = get_idatabase(database)) {
        return idb->table(std::string{table});
    }
    return nullptr;
}

} // namespace mnemo::databases