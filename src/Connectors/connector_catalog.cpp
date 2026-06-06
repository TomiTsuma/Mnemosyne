// src/Connectors/connector_catalog.cpp

#include "Connectors/connector_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::connectors {

namespace {

auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto connector_type_name(ConnectorType type) -> std::string {
    switch (type) {
        case ConnectorType::Rest:     return "REST";
        case ConnectorType::Postgres: return "POSTGRES";
        case ConnectorType::S3:       return "S3";
    }
    return "UNKNOWN";
}

auto connector_status_name(ConnectorStatus status) -> std::string {
    switch (status) {
        case ConnectorStatus::Creating:      return "CREATING";
        case ConnectorStatus::Validating:      return "VALIDATING";
        case ConnectorStatus::Active:          return "ACTIVE";
        case ConnectorStatus::Degraded:        return "DEGRADED";
        case ConnectorStatus::Disconnected:    return "DISCONNECTED";
        case ConnectorStatus::Disabled:        return "DISABLED";
        case ConnectorStatus::Archived:        return "ARCHIVED";
    }
    return "UNKNOWN";
}

auto auth_method_name(AuthMethod auth) -> std::string {
    switch (auth) {
        case AuthMethod::Anonymous:          return "ANONYMOUS";
        case AuthMethod::UsernamePassword:   return "USERNAME_PASSWORD";
        case AuthMethod::ApiKey:             return "API_KEY";
        case AuthMethod::OAuth2:             return "OAUTH2";
        case AuthMethod::Jwt:                return "JWT";
        case AuthMethod::Certificate:        return "CERTIFICATE";
        case AuthMethod::IamRole:            return "IAM_ROLE";
    }
    return "UNKNOWN";
}

auto parse_connector_type(std::string_view name) -> ConnectorType {
    const auto upper = to_upper(std::string{name});
    if (upper == "REST") return ConnectorType::Rest;
    if (upper == "POSTGRES" || upper == "POSTGRESQL") return ConnectorType::Postgres;
    if (upper == "S3") return ConnectorType::S3;
    throw common::Exception{
        "Unknown connector type: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_auth_method(std::string_view name) -> AuthMethod {
    const auto upper = to_upper(std::string{name});
    if (upper == "ANONYMOUS") return AuthMethod::Anonymous;
    if (upper == "USERNAME_PASSWORD" || upper == "USER_PASSWORD") return AuthMethod::UsernamePassword;
    if (upper == "API_KEY") return AuthMethod::ApiKey;
    if (upper == "OAUTH2") return AuthMethod::OAuth2;
    if (upper == "JWT") return AuthMethod::Jwt;
    if (upper == "CERTIFICATE") return AuthMethod::Certificate;
    if (upper == "IAM_ROLE" || upper == "IAM") return AuthMethod::IamRole;
    throw common::Exception{
        "Unknown auth method: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto capabilities_for_type(ConnectorType type) -> std::vector<std::string> {
    switch (type) {
        case ConnectorType::Rest:
            return {"READ", "SCHEMA_DISCOVERY"};
        case ConnectorType::Postgres:
            return {"READ", "WRITE", "SCHEMA_DISCOVERY", "CDC"};
        case ConnectorType::S3:
            return {"READ", "WRITE"};
    }
    return {};
}

auto is_secret_property(std::string_view key) -> bool {
    const auto upper = to_upper(std::string{key});
    return upper == "PASSWORD" || upper == "API_KEY" || upper == "SECRET_KEY"
        || upper == "ACCESS_KEY" || upper == "TOKEN" || upper == "SECRET";
}

auto mask_property_value(std::string_view key, std::string_view value) -> std::string {
    if (value.empty()) return "";
    return is_secret_property(key) ? "***" : std::string{value};
}

void validate_connector_entry(const ConnectorEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Connector name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    switch (entry.type) {
        case ConnectorType::Rest: {
            const auto it = entry.properties.find("BASE_URL");
            if (it == entry.properties.end() || it->second.empty()) {
                throw common::Exception{
                    "REST connector requires BASE_URL",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            break;
        }
        case ConnectorType::Postgres: {
            static const char* required[] = {"HOST", "PORT", "DATABASE", "USER"};
            for (const auto* key : required) {
                const auto it = entry.properties.find(key);
                if (it == entry.properties.end() || it->second.empty()) {
                    throw common::Exception{
                        std::string{"POSTGRES connector requires "} + key,
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
            }
            break;
        }
        case ConnectorType::S3: {
            const auto bucket = entry.properties.find("BUCKET");
            const auto endpoint = entry.properties.find("ENDPOINT");
            if (bucket == entry.properties.end() || bucket->second.empty()) {
                throw common::Exception{
                    "S3 connector requires BUCKET",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            if (endpoint == entry.properties.end() || endpoint->second.empty()) {
                throw common::Exception{
                    "S3 connector requires ENDPOINT",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            break;
        }
    }
}

} // namespace mnemo::connectors
