// src/StorageUnits/disk_factory.cpp — Disk factory implementation

#include "StorageUnits/disk_factory.h"
#include "Disks/disk_local.h"
#include "Disks/disk_s3.h"
#include "Common/exceptions.h"
#include <filesystem>

namespace mnemo::storage_units {

auto DiskFactory::create_disk(const StorageUnitEntry& entry) -> std::shared_ptr<disks::IDisk> {
    switch (entry.type) {
        case StorageUnitType::Local: {
            std::filesystem::create_directories(entry.path);
            return disks::LocalFileDisk::create(entry.name, entry.path);
        }
        case StorageUnitType::S3:
            return disks::S3Disk::create(
                entry.name,
                entry.endpoint,
                entry.bucket,
                entry.access_key,
                entry.secret_key,
                entry.region);
    }
    throw common::Exception{
        "Unsupported storage unit type",
        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

} // namespace mnemo::storage_units
