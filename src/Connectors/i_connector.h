// src/Connectors/i_connector.h — Connector driver interface

#pragma once

#include "Connectors/connector_catalog.h"
#include "Core/block.h"
#include <cstddef>
#include <string>
#include <vector>

namespace mnemo::connectors {

class IConnector {
public:
    virtual ~IConnector() = default;

    [[nodiscard]] virtual auto test(ConnectorEntry& entry) -> ConnectorTestResult = 0;
    [[nodiscard]] virtual auto discover_schema(const ConnectorEntry& entry)
        -> std::vector<std::string> = 0;
    [[nodiscard]] virtual auto read(const ConnectorEntry& entry, std::string_view resource,
                                    size_t limit) -> core::Block = 0;
    /// Writes every row of `data` to `resource`, as one request so the export is all-or-nothing
    /// from this side, and returns how many rows were sent. APPEND adds to what is there;
    /// OVERWRITE replaces it.
    [[nodiscard]] virtual auto write(const ConnectorEntry& entry, std::string_view resource,
                                     const core::Block& data, ExportMode mode) -> size_t = 0;
};

} // namespace mnemo::connectors
