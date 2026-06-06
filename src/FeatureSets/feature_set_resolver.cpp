// src/FeatureSets/feature_set_resolver.cpp

#include "FeatureSets/feature_set_resolver.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::feature_sets {

auto resolve_columns(const FeatureSetEntry& fs) -> std::vector<std::string> {
    std::vector<std::string> cols;
    cols.reserve(fs.features.size() + 2);
    if (!fs.entity_key.empty()) cols.push_back(fs.entity_key);
    for (const auto& f : fs.features) {
        if (f != fs.entity_key) cols.push_back(f);
    }
    if (!fs.target.empty() &&
        std::find(cols.begin(), cols.end(), fs.target) == cols.end()) {
        cols.push_back(fs.target);
    }
    return cols;
}

auto resolve_block(storages::IStorage& storage, const FeatureSetEntry& fs) -> core::Block {
    auto cols = resolve_columns(fs);

    // Validate the declared columns actually exist in the source table.
    auto available = storage.columns();
    for (const auto& c : cols) {
        if (std::find(available.begin(), available.end(), c) == available.end()) {
            throw common::Exception{
                "Feature set '" + fs.name + "' references unknown column '" + c +
                    "' in source table '" + fs.source_table + "'",
                static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
        }
    }

    if (cols.empty()) {
        // No declared columns — read everything (unsupervised / raw dataset).
        cols = available;
    }
    return storage.read(cols);
}

} // namespace mnemo::feature_sets
