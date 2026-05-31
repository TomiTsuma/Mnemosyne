// src/Databases/database_memory.cpp — Memory database engine implementation

#include "database_memory.h"
#include "database_factory.h"
#include "Storages/memory_storage.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::databases {

auto DatabaseMemory::create(std::string name, std::string path)
    -> std::shared_ptr<DatabaseMemory> {
    auto db = std::shared_ptr<DatabaseMemory>(new DatabaseMemory());
    db->name_ = std::move(name);
    db->path_ = std::move(path);
    return db;
}

DatabaseMemory::DatabaseMemory() = default;

auto DatabaseMemory::name() const -> std::string { return name_; }
auto DatabaseMemory::path() const -> std::string { return path_; }
auto DatabaseMemory::engine() const -> std::string { return BuiltinEngines::MEMORY; }

auto DatabaseMemory::tables() const -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& [table_name, _] : tables_) {
        names.push_back(table_name);
    }
    return names;
}

auto DatabaseMemory::attach_table(std::string name, std::shared_ptr<storages::IStorage> table) -> void {
    std::lock_guard lock(mutex_);
    tables_[std::move(name)] = std::move(table);
}

auto DatabaseMemory::detach_table(std::string name) -> std::shared_ptr<storages::IStorage> {
    std::lock_guard lock(mutex_);
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        return nullptr;
    }
    auto table = it->second;
    tables_.erase(it);
    return table;
}

auto DatabaseMemory::rename_table(std::string from, std::string to) -> bool {
    std::lock_guard lock(mutex_);
    auto it = tables_.find(from);
    if (it == tables_.end()) {
        return false;
    }
    if (tables_.find(to) != tables_.end()) {
        return false;
    }
    tables_[std::move(to)] = it->second;
    tables_.erase(it);
    return true;
}

auto DatabaseMemory::table_exists(std::string_view name) const -> bool {
    std::lock_guard lock(mutex_);
    return tables_.find(std::string{name}) != tables_.end();
}

auto DatabaseMemory::table(std::string name) -> std::shared_ptr<storages::IStorage> {
    std::lock_guard lock(mutex_);
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        return nullptr;
    }
    return it->second;
}

auto DatabaseMemory::drop_table(std::string name) -> bool {
    std::lock_guard lock(mutex_);
    return tables_.erase(name) > 0;
}

auto DatabaseMemory::create_table(
    std::string name,
    std::unordered_map<std::string, datatypes::DataTypePtr> columns,
    std::string engine) -> std::shared_ptr<storages::IStorage> {
    std::lock_guard lock(mutex_);

    if (tables_.find(name) != tables_.end()) {
        throw common::Exception{
            "DatabaseMemory::create_table: table already exists: " + name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    if (engine != BuiltinEngines::MEMORY && engine != "Memory") {
        throw common::Exception{
            "DatabaseMemory::create_table: unsupported engine '" + engine + "'",
            static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
    }

    auto storage = storages::MemoryStorage::create(name);
    for (auto& [col_name, type] : columns) {
        storage->add_column(std::move(col_name), std::move(type));
    }
    tables_[name] = storage;
    return storage;
}

void DatabaseMemory::clear_all() {
    std::lock_guard lock(mutex_);
    tables_.clear();
}

} // namespace mnesso::databases
