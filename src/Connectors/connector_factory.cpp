// src/Connectors/connector_factory.cpp

#include "Connectors/connector_factory.h"
#include "Connectors/rest_connector.h"
#include "Connectors/postgres_connector.h"
#include "Connectors/s3_connector.h"
#include "Common/exceptions.h"

namespace mnemo::connectors {

auto ConnectorFactory::create_driver(ConnectorType type) -> std::unique_ptr<IConnector> {
    switch (type) {
        case ConnectorType::Rest:
            return std::make_unique<RestConnector>();
        case ConnectorType::Postgres:
            return std::make_unique<PostgresConnector>();
        case ConnectorType::S3:
            return std::make_unique<S3Connector>();
    }
    throw common::Exception{
        "Unsupported connector type",
        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

} // namespace mnemo::connectors
