// src/FeatureSets/feature_set_resolver.h — resolve a FEATURE_SET into a Block
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "FeatureSets/feature_set_catalog.h"
#include "Storages/i_storage.h"
#include <string>
#include <vector>

namespace mnemo::feature_sets {

// ── Column list (ordered) a feature set materialises: entity_key, features..., target ──
[[nodiscard]] auto resolve_columns(const FeatureSetEntry& fs) -> std::vector<std::string>;

// ── Read the feature set's declared columns from a storage into a Block ──
// The caller resolves the storage (the feature set's source table) and passes
// it in; this keeps the FeatureSets library free of an Interpreters dependency.
[[nodiscard]] auto resolve_block(storages::IStorage& storage, const FeatureSetEntry& fs)
    -> core::Block;

} // namespace mnemo::feature_sets
