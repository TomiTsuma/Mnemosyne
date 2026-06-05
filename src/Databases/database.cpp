// src/Databases/database.cpp — Database implementation
// Mnemosyne: A column-oriented analytical DBMS

#include <algorithm>
#include "Common/exceptions.h"
#include "DataTypes/data_type_factory.h"
#include "Storages/memory_storage.h"
#include "Storages/storage_factory.h"
#include "Storages/table.h"
#include "database.h"

namespace mnesso::databases {

// ── Database ──

Database::Database(std::string name) : name_{std::move(name)} {}

// IDatabase interface

auto Database::name() const -> std::string {
    return name_;
}

auto Database::path() const -> std::string {
    return path_;
}

auto Database::engine() const -> std::string {
    return engine_;
}

auto Database::tables() const -> std::vector<std::string> {
    return table_names();
}

auto Database::attach_table(std::string name, std::shared_ptr<storages::IStorage> table) -> void {
    auto tbl             = std::make_shared<storages::Table>(std::move(name));
    tbl->storage()       = std::move(table);
    tables_[tbl->name()] = tbl;
}

auto Database::detach_table(std::string name) -> std::shared_ptr<storages::IStorage> {
    auto it = tables_.find(std::move(name));
    if (it == tables_.end())
        return nullptr;
    auto storage = it->second->storage();
    tables_.erase(it);
    return storage;
}

auto Database::rename_table(std::string from, std::string to) -> bool {
    auto it = tables_.find(std::move(from));
    if (it == tables_.end())
        return false;
    auto storage = it->second->storage();
    tables_.erase(it);
    auto tbl       = std::make_shared<storages::Table>(std::move(to));
    tbl->storage() = storage;
    tables_.insert({tbl->name(), tbl});
    return true;
}

auto Database::table_exists(std::string_view name) const -> bool {
    return has_table(name);
}

auto Database::table(std::string name) -> std::shared_ptr<storages::IStorage> {
    auto tbl = get_table(std::move(name));
    return tbl ? tbl->storage() : nullptr;
}

auto Database::drop_table(std::string name) -> bool {
    tables_.erase(std::move(name));
    return true;
}

auto Database::create_table(std::string                                             name,
                            std::unordered_map<std::string, datatypes::DataTypePtr> columns,
                            std::string engine) -> std::shared_ptr<storages::IStorage> {
    auto tbl = std::make_shared<storages::Table>(std::move(name));
    for (const auto& [col_name, type] : columns) {
        tbl->add_column(col_name, std::move(type));
    }
    // auto storage = storages::MemoryStorage::create(engine); This is for only memory storage
    auto storage   = mnesso::storages::StorageFactory::instance().create(std::move(name), engine);
    tbl->storage() = storage;
    tables_[tbl->name()] = tbl;
    return storage;
}

// Database-specific methods

auto Database::get_table(std::string_view name) -> std::shared_ptr<storages::Table> {
    auto it = tables_.find(std::string{name});
    if (it == tables_.end())
        return nullptr;
    return it->second;
}

auto Database::get_table(std::string_view name) const -> std::shared_ptr<storages::Table> {
    auto it = tables_.find(std::string{name});
    if (it == tables_.end())
        return nullptr;
    return it->second;
}

void Database::create_table(std::string name, std::vector<ColumnDef> columns) {
    auto table = std::make_shared<storages::Table>(std::move(name));
    for (const auto& col : columns) {
        auto dt = mnesso::datatypes::get_data_type(col.data_type);
        table->add_column(col.name, dt);
    }
    tables_[table->name()] = table;
}

auto Database::table_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& [name, _] : tables_) {
        names.push_back(name);
    }
    return names;
}

size_t Database::table_count() const {
    return tables_.size();
}

bool Database::has_table(std::string_view name) const {
    return tables_.find(std::string{name}) != tables_.end();
}

} // namespace mnesso::databases