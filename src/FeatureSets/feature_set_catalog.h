// src/FeatureSets/feature_set_catalog.h — FEATURE_SET / DATASET catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace mnemo::feature_sets {

// ── FeatureSetEntry — an explicitly-declared set of model inputs ──
// Per MODEL_LAYER PRD Principle 2 (No Implicit Feature Discovery), the
// entity key, features and target are always declared, never inferred.
struct FeatureSetEntry {
    std::string name;
    std::string source_table;            // optional FROM <table>
    std::string entity_key;
    std::vector<std::string> features;
    std::string target;                  // empty for unsupervised / LLM
    uint32_t version = 1;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

// ── DatasetEntry — raw training data reference (deep learning / LLM / RL) ──
struct DatasetEntry {
    std::string name;
    std::string source;                  // table name or filesystem path
    uint32_t version = 1;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

} // namespace mnemo::feature_sets
