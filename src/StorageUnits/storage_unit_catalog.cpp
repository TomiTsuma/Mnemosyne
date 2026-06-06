// src/StorageUnits/storage_unit_catalog.cpp — Storage unit catalog helpers

#include "StorageUnits/storage_unit_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace mnemo::storage_units {

namespace {

auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto storage_unit_type_name(StorageUnitType type) -> std::string {
    switch (type) {
        case StorageUnitType::Local: return "LOCAL";
        case StorageUnitType::S3:    return "S3";
    }
    return "UNKNOWN";
}

auto storage_unit_status_name(StorageUnitStatus status) -> std::string {
    switch (status) {
        case StorageUnitStatus::Online:       return "ONLINE";
        case StorageUnitStatus::Degraded:     return "DEGRADED";
        case StorageUnitStatus::Offline:      return "OFFLINE";
        case StorageUnitStatus::Maintenance:  return "MAINTENANCE";
    }
    return "UNKNOWN";
}

auto parse_storage_unit_type(std::string_view name) -> StorageUnitType {
    const auto upper = to_upper(std::string{name});
    if (upper == "LOCAL") return StorageUnitType::Local;
    if (upper == "S3") return StorageUnitType::S3;
    throw common::Exception{
        "Unknown storage unit type: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto capabilities_for_type(StorageUnitType type) -> std::vector<std::string> {
    switch (type) {
        case StorageUnitType::Local:
            return {"BLOCK_STORAGE"};
        case StorageUnitType::S3:
            return {"OBJECT_STORAGE"};
    }
    return {};
}

void validate_storage_unit_entry(const StorageUnitEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Storage unit name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    switch (entry.type) {
        case StorageUnitType::Local:
            if (entry.path.empty()) {
                throw common::Exception{
                    "LOCAL storage unit requires PATH",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            break;
        case StorageUnitType::S3:
            if (entry.bucket.empty()) {
                throw common::Exception{
                    "S3 storage unit requires BUCKET",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            break;
    }
}

} // namespace mnemo::storage_units
