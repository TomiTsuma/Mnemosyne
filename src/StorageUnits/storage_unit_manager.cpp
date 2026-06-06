// src/StorageUnits/storage_unit_manager.cpp — Storage unit manager implementation

#include "StorageUnits/storage_unit_manager.h"
#include "StorageUnits/disk_factory.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::storage_units {

auto StorageUnitManager::instance() -> StorageUnitManager& {
    static StorageUnitManager inst;
    return inst;
}

auto StorageUnitManager::create_unit(StorageUnitEntry entry) -> void {
    validate_storage_unit_entry(entry);

    if (units_.contains(entry.name)) {
        throw common::Exception{
            "Storage unit already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    entry.disk = DiskFactory::create_disk(entry);

    if (entry.disk) {
        const auto stats = entry.disk->stats();
        entry.capacity_bytes = stats.total_space;
        entry.used_bytes = stats.space_used;
        entry.available_bytes = stats.free_space;
    }

    units_.emplace(entry.name, std::move(entry));
}

auto StorageUnitManager::get_unit(std::string_view name) -> StorageUnitEntry* {
    auto it = units_.find(std::string{name});
    if (it == units_.end()) return nullptr;
    return &it->second;
}

auto StorageUnitManager::get_unit(std::string_view name) const -> const StorageUnitEntry* {
    auto it = units_.find(std::string{name});
    if (it == units_.end()) return nullptr;
    return &it->second;
}

auto StorageUnitManager::has_unit(std::string_view name) const -> bool {
    return units_.contains(std::string{name});
}

auto StorageUnitManager::list_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(units_.size());
    for (const auto& [name, _] : units_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

auto StorageUnitManager::drop_unit(std::string name, bool if_exists) -> void {
    auto it = units_.find(name);
    if (it == units_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown storage unit: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (it->second.reference_count > 0) {
        throw common::Exception{
            "Storage unit is in use: " + name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    units_.erase(it);
}

auto StorageUnitManager::acquire_disk(std::string_view name) -> std::shared_ptr<disks::IDisk> {
    auto* entry = get_unit(name);
    if (!entry || !entry->disk) {
        throw common::Exception{
            "Unknown storage unit: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    ++entry->reference_count;
    return entry->disk;
}

auto StorageUnitManager::release_disk(std::string_view name) -> void {
    auto* entry = get_unit(name);
    if (!entry) return;
    if (entry->reference_count > 0) {
        --entry->reference_count;
    }
}

auto StorageUnitManager::refresh_stats(std::string_view name) -> void {
    auto* entry = get_unit(name);
    if (!entry || !entry->disk) return;
    const auto stats = entry->disk->stats();
    entry->capacity_bytes = stats.total_space;
    entry->used_bytes = stats.space_used;
    entry->available_bytes = stats.free_space;
    entry->updated_at = std::chrono::system_clock::now();
}

auto StorageUnitManager::list_entries() const -> std::vector<StorageUnitEntry> {
    std::vector<StorageUnitEntry> result;
    result.reserve(units_.size());
    for (const auto& [_, entry] : units_) {
        result.push_back(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const StorageUnitEntry& a, const StorageUnitEntry& b) {
                  return a.name < b.name;
              });
    return result;
}

} // namespace mnemo::storage_units
