// src/Connectors/rest_connector.h

#pragma once

#include "Connectors/i_connector.h"

namespace mnemo::connectors {

class RestConnector final : public IConnector {
public:
    [[nodiscard]] auto test(ConnectorEntry& entry) -> ConnectorTestResult override;
    [[nodiscard]] auto discover_schema(const ConnectorEntry& entry)
        -> std::vector<std::string> override;
    [[nodiscard]] auto read(const ConnectorEntry& entry, std::string_view resource,
                            size_t limit) -> core::Block override;
    /// APPEND sends the rows with POST, OVERWRITE with PUT, both as a JSON array of objects to
    /// <BASE_URL>/<resource>. Any non-2xx response is an error.
    [[nodiscard]] auto write(const ConnectorEntry& entry, std::string_view resource,
                             const core::Block& data, ExportMode mode) -> size_t override;
};

} // namespace mnemo::connectors
