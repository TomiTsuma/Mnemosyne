// src/StorageUnits/storage_unit_catalog.h — Storage unit catalog types
// Mnemosyne: A column-oriented analytical DBMS
//
// STORAGE_UNIT (PRD) is a named, cluster-wide storage resource — distinct from
// storages::IStorage (table engine) and disks::IDisk (raw filesystem backend).

#pragma once

#include "Disks/disk.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mnemo::storage_units {

enum class StorageUnitType { Local, S3 };

enum class StorageUnitStatus { Online, Degraded, Offline, Maintenance };

[[nodiscard]] auto storage_unit_type_name(StorageUnitType type) -> std::string;
[[nodiscard]] auto storage_unit_status_name(StorageUnitStatus status) -> std::string;
[[nodiscard]] auto parse_storage_unit_type(std::string_view name) -> StorageUnitType;
[[nodiscard]] auto capabilities_for_type(StorageUnitType type) -> std::vector<std::string>;

struct StorageUnitEntry {
    std::string name;
    StorageUnitType type = StorageUnitType::Local;
    StorageUnitStatus status = StorageUnitStatus::Online;

    std::string path;
    std::string endpoint;
    std::string bucket;
    std::string region;
    std::string access_key;
    std::string secret_key;

    uint64_t capacity_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t available_bytes = 0;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    std::string owner;

    std::shared_ptr<disks::IDisk> disk;
    size_t reference_count = 0;
};

void validate_storage_unit_entry(const StorageUnitEntry& entry);

} // namespace mnemo::storage_units
