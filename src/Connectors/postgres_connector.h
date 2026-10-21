// src/Connectors/postgres_connector.h

#pragma once

#include "Connectors/i_connector.h"

namespace mnemo::connectors {

class PostgresConnector final : public IConnector {
public:
    [[nodiscard]] auto test(ConnectorEntry& entry) -> ConnectorTestResult override;
    [[nodiscard]] auto discover_schema(const ConnectorEntry& entry)
        -> std::vector<std::string> override;
    [[nodiscard]] auto read(const ConnectorEntry& entry, std::string_view resource,
                            size_t limit) -> core::Block override;
    [[nodiscard]] auto write(const ConnectorEntry& entry, std::string_view resource,
                             const core::Block& data, ExportMode mode) -> size_t override;
};

} // namespace mnemo::connectors
