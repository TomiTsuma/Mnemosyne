// src/Databases/database.cpp — Database implementation
// Mnemosyne: A column-oriented analytical DBMS

#include <algorithm>
#include "Common/exceptions.h"
#include "DataTypes/data_type_factory.h"
#include "Storages/memory_storage.h"
#include "Storages/storage_factory.h"
#include "Storages/table.h"
#include "database.h"

namespace mnemo::databases {

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
    const std::string table_name = name;
    auto tbl = std::make_shared<storages::Table>(table_name);
    auto storage = mnemo::storages::StorageFactory::instance().create(table_name, engine);
    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->set_columns(columns);
    }
    tbl->set_storage(storage);
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
        auto dt = mnemo::datatypes::get_data_type(col.data_type);
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

auto Database::has_view(std::string_view name) const -> bool {
    return views_.contains(std::string{name});
}

auto Database::has_materialized_view(std::string_view name) const -> bool {
    return materialized_views_.contains(std::string{name});
}

auto Database::get_view(std::string_view name) const -> std::optional<ViewEntry> {
    auto it = views_.find(std::string{name});
    if (it == views_.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto Database::get_materialized_view(std::string_view name) const
    -> std::optional<MaterializedViewEntry> {
    auto it = materialized_views_.find(std::string{name});
    if (it == materialized_views_.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto Database::view_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(views_.size());
    for (const auto& [name, _] : views_) {
        names.push_back(name);
    }
    return names;
}

auto Database::materialized_view_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(materialized_views_.size());
    for (const auto& [name, _] : materialized_views_) {
        names.push_back(name);
    }
    return names;
}

auto Database::create_view(ViewEntry entry) -> void {
    views_[entry.name] = std::move(entry);
}

auto Database::drop_view(std::string name) -> bool {
    return views_.erase(name) > 0;
}

auto Database::create_materialized_view(MaterializedViewEntry entry) -> void {
    materialized_views_[entry.name] = std::move(entry);
}

auto Database::drop_materialized_view(std::string name) -> bool {
    return materialized_views_.erase(name) > 0;
}

auto Database::relation_exists(std::string_view name) const -> bool {
    const auto key = std::string{name};
    return tables_.contains(key) || views_.contains(key) || materialized_views_.contains(key);
}

} // namespace mnemo::databases