// src/StorageUnits/storage_unit_manager.h — Cluster-wide storage unit registry

#pragma once

#include "StorageUnits/storage_unit_catalog.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::storage_units {

class StorageUnitManager {
public:
    static auto instance() -> StorageUnitManager&;

    auto create_unit(StorageUnitEntry entry) -> void;
    auto get_unit(std::string_view name) -> StorageUnitEntry*;
    auto get_unit(std::string_view name) const -> const StorageUnitEntry*;
    [[nodiscard]] auto has_unit(std::string_view name) const -> bool;
    [[nodiscard]] auto list_names() const -> std::vector<std::string>;
    auto drop_unit(std::string name, bool if_exists) -> void;

    auto acquire_disk(std::string_view name) -> std::shared_ptr<disks::IDisk>;
    auto release_disk(std::string_view name) -> void;
    auto refresh_stats(std::string_view name) -> void;

    [[nodiscard]] auto list_entries() const -> std::vector<StorageUnitEntry>;

private:
    StorageUnitManager() = default;

    std::unordered_map<std::string, StorageUnitEntry> units_;
};

} // namespace mnemo::storage_units
