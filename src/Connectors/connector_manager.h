// src/Connectors/connector_manager.h — Cluster-wide connector registry

#pragma once

#include "Connectors/connector_catalog.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::connectors {

class ConnectorManager {
public:
    static auto instance() -> ConnectorManager&;

    auto create_connector(ConnectorEntry entry) -> void;
    auto get_connector(std::string_view name) -> ConnectorEntry*;
    auto get_connector(std::string_view name) const -> const ConnectorEntry*;
    [[nodiscard]] auto has_connector(std::string_view name) const -> bool;
    [[nodiscard]] auto list_names() const -> std::vector<std::string>;
    auto drop_connector(std::string name, bool if_exists) -> void;
    auto alter_connector(std::string_view name,
                         const std::vector<std::pair<std::string, std::string>>& sets) -> void;
    auto update_test_result(std::string_view name, ConnectorTestResult result,
                            ConnectorStatus status) -> void;
    [[nodiscard]] auto list_entries() const -> std::vector<ConnectorEntry>;

private:
    ConnectorManager() = default;

    std::unordered_map<std::string, ConnectorEntry> connectors_;
};

} // namespace mnemo::connectors
