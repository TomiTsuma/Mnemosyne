// src/FeatureSets/feature_set_manager.cpp

#include "FeatureSets/feature_set_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::feature_sets {

auto FeatureSetManager::instance() -> FeatureSetManager& {
    static FeatureSetManager inst;
    return inst;
}

auto FeatureSetManager::create_feature_set(FeatureSetEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (entry.name.empty()) {
        throw common::Exception{
            "FEATURE_SET name must not be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (feature_sets_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Feature set already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    if (entry.version == 0) entry.version = 1;
    feature_sets_.emplace(entry.name, std::move(entry));
}

auto FeatureSetManager::drop_feature_set(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = feature_sets_.find(name);
    if (it == feature_sets_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown feature set: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    feature_sets_.erase(it);
}

auto FeatureSetManager::get_feature_set(std::string_view name) const -> const FeatureSetEntry* {
    std::lock_guard lock{mutex_};
    auto it = feature_sets_.find(std::string{name});
    if (it == feature_sets_.end()) return nullptr;
    return &it->second;
}

auto FeatureSetManager::has_feature_set(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return feature_sets_.contains(std::string{name});
}

auto FeatureSetManager::list_feature_sets() const -> std::vector<FeatureSetEntry> {
    std::lock_guard lock{mutex_};
    std::vector<FeatureSetEntry> result;
    result.reserve(feature_sets_.size());
    for (const auto& [_, entry] : feature_sets_) result.push_back(entry);
    std::sort(result.begin(), result.end(),
              [](const FeatureSetEntry& a, const FeatureSetEntry& b) { return a.name < b.name; });
    return result;
}

auto FeatureSetManager::bump_feature_set_version(std::string_view name) -> uint32_t {
    std::lock_guard lock{mutex_};
    auto it = feature_sets_.find(std::string{name});
    if (it == feature_sets_.end()) {
        throw common::Exception{
            "Unknown feature set: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    it->second.version += 1;
    it->second.updated_at = std::chrono::system_clock::now();
    return it->second.version;
}

auto FeatureSetManager::create_dataset(DatasetEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (entry.name.empty()) {
        throw common::Exception{
            "DATASET name must not be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (datasets_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Dataset already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    if (entry.version == 0) entry.version = 1;
    datasets_.emplace(entry.name, std::move(entry));
}

auto FeatureSetManager::drop_dataset(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown dataset: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    datasets_.erase(it);
}

auto FeatureSetManager::get_dataset(std::string_view name) const -> const DatasetEntry* {
    std::lock_guard lock{mutex_};
    auto it = datasets_.find(std::string{name});
    if (it == datasets_.end()) return nullptr;
    return &it->second;
}

auto FeatureSetManager::has_dataset(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return datasets_.contains(std::string{name});
}

auto FeatureSetManager::list_datasets() const -> std::vector<DatasetEntry> {
    std::lock_guard lock{mutex_};
    std::vector<DatasetEntry> result;
    result.reserve(datasets_.size());
    for (const auto& [_, entry] : datasets_) result.push_back(entry);
    std::sort(result.begin(), result.end(),
              [](const DatasetEntry& a, const DatasetEntry& b) { return a.name < b.name; });
    return result;
}

} // namespace mnemo::feature_sets
