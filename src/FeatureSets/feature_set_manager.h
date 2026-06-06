// src/FeatureSets/feature_set_manager.h — FEATURE_SET / DATASET registry
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "FeatureSets/feature_set_catalog.h"
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::feature_sets {

class FeatureSetManager {
public:
    static auto instance() -> FeatureSetManager&;

    // ── FEATURE_SET ──
    auto create_feature_set(FeatureSetEntry entry, bool if_not_exists) -> void;
    auto drop_feature_set(std::string name, bool if_exists) -> void;
    [[nodiscard]] auto get_feature_set(std::string_view name) const -> const FeatureSetEntry*;
    [[nodiscard]] auto has_feature_set(std::string_view name) const -> bool;
    [[nodiscard]] auto list_feature_sets() const -> std::vector<FeatureSetEntry>;
    auto bump_feature_set_version(std::string_view name) -> uint32_t;

    // ── DATASET ──
    auto create_dataset(DatasetEntry entry, bool if_not_exists) -> void;
    auto drop_dataset(std::string name, bool if_exists) -> void;
    [[nodiscard]] auto get_dataset(std::string_view name) const -> const DatasetEntry*;
    [[nodiscard]] auto has_dataset(std::string_view name) const -> bool;
    [[nodiscard]] auto list_datasets() const -> std::vector<DatasetEntry>;

private:
    FeatureSetManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, FeatureSetEntry> feature_sets_;
    std::unordered_map<std::string, DatasetEntry> datasets_;
};

} // namespace mnemo::feature_sets
