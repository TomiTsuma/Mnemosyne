// src/Connectors/connector_catalog.h — Connector catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::connectors {

enum class ConnectorType { Rest, Postgres, S3 };

enum class ConnectorStatus {
    Creating,
    Validating,
    Active,
    Degraded,
    Disconnected,
    Disabled,
    Archived
};

enum class AuthMethod {
    Anonymous,
    UsernamePassword,
    ApiKey,
    OAuth2,
    Jwt,
    Certificate,
    IamRole
};

struct ConnectorTestResult {
    bool ok = false;
    uint64_t latency_ms = 0;
    std::string message;
    std::chrono::system_clock::time_point tested_at{};
};

struct ConnectorEntry {
    std::string name;
    ConnectorType type = ConnectorType::Rest;
    ConnectorStatus status = ConnectorStatus::Creating;
    AuthMethod auth = AuthMethod::Anonymous;
    std::unordered_map<std::string, std::string> properties;
    std::vector<std::string> capabilities;
    std::string owner;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    ConnectorTestResult last_test;
};

[[nodiscard]] auto connector_type_name(ConnectorType type) -> std::string;
[[nodiscard]] auto connector_status_name(ConnectorStatus status) -> std::string;
[[nodiscard]] auto auth_method_name(AuthMethod auth) -> std::string;
[[nodiscard]] auto parse_connector_type(std::string_view name) -> ConnectorType;
[[nodiscard]] auto parse_auth_method(std::string_view name) -> AuthMethod;
[[nodiscard]] auto capabilities_for_type(ConnectorType type) -> std::vector<std::string>;
[[nodiscard]] auto is_secret_property(std::string_view key) -> bool;
[[nodiscard]] auto mask_property_value(std::string_view key, std::string_view value) -> std::string;

void validate_connector_entry(const ConnectorEntry& entry);

} // namespace mnemo::connectors
