// src/Connectors/s3_connector.h

#pragma once

#include "Connectors/i_connector.h"

namespace mnemo::connectors {

class S3Connector final : public IConnector {
public:
    [[nodiscard]] auto test(ConnectorEntry& entry) -> ConnectorTestResult override;
    [[nodiscard]] auto discover_schema(const ConnectorEntry& entry)
        -> std::vector<std::string> override;
    [[nodiscard]] auto read(const ConnectorEntry& entry, std::string_view resource,
                            size_t limit) -> core::Block override;
};

} // namespace mnemo::connectors
