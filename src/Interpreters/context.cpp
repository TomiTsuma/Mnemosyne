// src/Interpreters/context.cpp — Context implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "context.h"

namespace mnemo::interpreters {

auto Context::get_storage(std::string_view name) -> std::shared_ptr<storages::IStorage> {
    auto it = storages_.find(static_cast<std::string>(name));
    return it != storages_.end() ? it->second : nullptr;
}

void Context::register_storage(std::string name,
                                 std::shared_ptr<storages::IStorage> storage) {
    storages_[std::move(name)] = std::move(storage);
}

void Context::register_database(std::string name,
                                 std::shared_ptr<databases::IDatabase> db) {
    databases_[std::move(name)] = std::move(db);
}

auto Context::get_database(std::string_view name)
    -> std::shared_ptr<databases::IDatabase> {
    auto it = databases_.find(static_cast<std::string>(name));
    return it != databases_.end() ? it->second : nullptr;
}

auto Context::databases() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(databases_.size());
    for (auto&& [k, _] : databases_) result.push_back(k);
    return result;
}

void Context::track_memory(size_t bytes) {
    memory_tracked_ += bytes;
}

void Context::untrack_memory(size_t bytes) {
    if (bytes <= memory_tracked_) memory_tracked_ -= bytes;
}

auto Context::total_memory() const -> size_t { return memory_tracked_; }

bool Context::has_role(std::string_view name) const {
    return roles_.count(static_cast<std::string>(name)) > 0;
}

auto Context::get_setting(std::string_view name)
    -> std::optional<common::SettingValueType> {
    return settings_.get(name);
}

void Context::set_setting(std::string_view name, common::SettingValueType value) {
    settings_.set(name, std::move(value));
}

Context::Context(std::shared_ptr<databases::IDatabase> db) {
    if (db) {
        databases_["default"] = std::move(db);
    }
}

} // namespace mnemo::interpreters
