// src/Models/model_runtime.h — bridge between the C++ catalog and the Python ML runtime
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Models/json.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mnemo::models {

// ── What the C++ side asks the Python runtime to do ──
struct RuntimeRequest {
    std::string action;          // train | tune | evaluate | predict | generate | explain
    std::string run_id;
    std::string model;
    std::string model_type;      // CLASSIFICATION / REGRESSION / ...
    std::string framework;       // SKLEARN / XGBOOST / ...
    std::string algorithm;
    std::string entrypoint;
    std::string objective;

    std::string entity_key;
    std::vector<std::string> features;
    std::string target;

    std::map<std::string, std::string> hyperparameters;

    // tuning
    std::string strategy;
    uint32_t trials = 0;
    std::map<std::string, std::string> search_space;

    // evaluate / predict
    std::string artifact_in;     // existing artifact to load
    std::string predict_mode;    // entity | features | batch
    std::map<std::string, std::string> predict_features; // for FOR/WITH

    // generate
    std::string prompt;

    // data attached as CSV (training/eval/batch). May be empty.
    const core::Block* data = nullptr;
};

struct TrialRecord {
    std::map<std::string, std::string> params;
    std::map<std::string, double> metrics;
    double objective_value = 0.0;
};

struct PredictionOut {
    std::string entity_key;
    std::string prediction;
    double confidence = 0.0;
};

struct RuntimeResult {
    bool ok = false;
    std::string error;
    std::map<std::string, double> metrics;
    std::string artifact_location;
    std::map<std::string, std::string> best_params;
    std::vector<TrialRecord> trials;
    std::vector<PredictionOut> predictions;
    std::string generated_text;
    json::Value raw;             // full parsed result.json for extra fields
};

// ── Convert a single Field to a CSV/string representation ──
[[nodiscard]] auto field_to_string(const core::Field& f) -> std::string;

class ModelRuntime {
public:
    // Run a request: builds a run dir, exports data, writes spec.json, spawns
    // the Python runtime, and parses result.json.
    static auto run(const RuntimeRequest& req) -> RuntimeResult;

    // Directory for a given run id (created on demand).
    [[nodiscard]] static auto run_dir(const std::string& run_id) -> std::string;
};

} // namespace mnemo::models
