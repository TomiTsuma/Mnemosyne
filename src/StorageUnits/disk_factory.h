// src/StorageUnits/disk_factory.h — Build IDisk instances from storage unit entries

#pragma once

#include "StorageUnits/storage_unit_catalog.h"
#include <memory>

namespace mnemo::storage_units {

class DiskFactory {
public:
    static auto create_disk(const StorageUnitEntry& entry) -> std::shared_ptr<disks::IDisk>;
};

} // namespace mnemo::storage_units
