// src/Connectors/s3_connector.cpp

#include "Connectors/s3_connector.h"
#include "Common/exceptions.h"
#include <chrono>

namespace mnemo::connectors {

auto S3Connector::test(ConnectorEntry& entry) -> ConnectorTestResult {
    ConnectorTestResult result;
    const auto started = std::chrono::steady_clock::now();
    try {
        validate_connector_entry(entry);
        result.ok = true;
        result.message = "S3 connector configuration valid (catalog only)";
    } catch (const common::Exception& e) {
        result.ok = false;
        result.message = e.what();
    }
    const auto ended = std::chrono::steady_clock::now();
    result.latency_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count());
    result.tested_at = std::chrono::system_clock::now();
    return result;
}

auto S3Connector::discover_schema(const ConnectorEntry& entry) -> std::vector<std::string> {
    (void)entry;
    throw common::Exception{
        "S3 schema discovery is not implemented in Phase 1",
        static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
}

auto S3Connector::read(const ConnectorEntry& entry, std::string_view resource,
                       size_t limit) -> core::Block {
    (void)entry;
    (void)resource;
    (void)limit;
    throw common::Exception{
        "SELECT FROM S3 connector is not implemented in Phase 1",
        static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
}

} // namespace mnemo::connectors
