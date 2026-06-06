// src/Connectors/connector_manager.cpp

#include "Connectors/connector_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::connectors {

auto ConnectorManager::instance() -> ConnectorManager& {
    static ConnectorManager inst;
    return inst;
}

auto ConnectorManager::create_connector(ConnectorEntry entry) -> void {
    validate_connector_entry(entry);

    if (connectors_.contains(entry.name)) {
        throw common::Exception{
            "Connector already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    entry.status = ConnectorStatus::Active;
    entry.capabilities = capabilities_for_type(entry.type);
    connectors_.emplace(entry.name, std::move(entry));
}

auto ConnectorManager::get_connector(std::string_view name) -> ConnectorEntry* {
    auto it = connectors_.find(std::string{name});
    if (it == connectors_.end()) return nullptr;
    return &it->second;
}

auto ConnectorManager::get_connector(std::string_view name) const -> const ConnectorEntry* {
    auto it = connectors_.find(std::string{name});
    if (it == connectors_.end()) return nullptr;
    return &it->second;
}

auto ConnectorManager::has_connector(std::string_view name) const -> bool {
    return connectors_.contains(std::string{name});
}

auto ConnectorManager::list_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(connectors_.size());
    for (const auto& [name, _] : connectors_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

auto ConnectorManager::drop_connector(std::string name, bool if_exists) -> void {
    auto it = connectors_.find(name);
    if (it == connectors_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown connector: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    connectors_.erase(it);
}

auto ConnectorManager::alter_connector(
    std::string_view name,
    const std::vector<std::pair<std::string, std::string>>& sets) -> void {
    auto* entry = get_connector(name);
    if (!entry) {
        throw common::Exception{
            "Unknown connector: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    for (const auto& [key, value] : sets) {
        entry->properties[key] = value;
    }
    validate_connector_entry(*entry);
    entry->updated_at = std::chrono::system_clock::now();
}

auto ConnectorManager::update_test_result(std::string_view name, ConnectorTestResult result,
                                          ConnectorStatus status) -> void {
    auto* entry = get_connector(name);
    if (!entry) {
        throw common::Exception{
            "Unknown connector: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    entry->last_test = std::move(result);
    entry->status = status;
    entry->updated_at = std::chrono::system_clock::now();
}

auto ConnectorManager::list_entries() const -> std::vector<ConnectorEntry> {
    std::vector<ConnectorEntry> result;
    result.reserve(connectors_.size());
    for (const auto& [_, entry] : connectors_) {
        result.push_back(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const ConnectorEntry& a, const ConnectorEntry& b) {
                  return a.name < b.name;
              });
    return result;
}

} // namespace mnemo::connectors
