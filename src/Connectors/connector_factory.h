// src/Connectors/connector_factory.h

#pragma once

#include "Connectors/connector_catalog.h"
#include "Connectors/i_connector.h"
#include <memory>

namespace mnemo::connectors {

class ConnectorFactory {
public:
    [[nodiscard]] static auto create_driver(ConnectorType type) -> std::unique_ptr<IConnector>;
};

} // namespace mnemo::connectors
