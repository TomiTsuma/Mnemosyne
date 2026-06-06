// src/Models/model_catalog.h — MODEL layer catalog entity types + enums
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::models {

// ── Enums (PRD §5, §11, §13, §17) ──
enum class ModelType {
    CLASSIFICATION, REGRESSION, FORECASTING, RECOMMENDATION,
    CLUSTERING, EMBEDDING, LLM, RL, CUSTOM
};

enum class Framework {
    SKLEARN, XGBOOST, LIGHTGBM, CATBOOST, PYTORCH, TENSORFLOW, CUSTOM
};

enum class RunStatus { QUEUED, RUNNING, SUCCEEDED, FAILED, CANCELLED };

enum class EndpointStatus { STARTING, ACTIVE, SCALING, FAILED, STOPPED };

enum class TuningStrategy { GRID, RANDOM, OPTUNA, HYPEROPT, BAYESIAN, EVOLUTIONARY };

// ── Enum <-> string helpers ──
[[nodiscard]] auto model_type_name(ModelType t) -> std::string_view;
[[nodiscard]] auto parse_model_type(std::string_view s, ModelType& out) -> bool;
[[nodiscard]] auto framework_name(Framework f) -> std::string_view;
[[nodiscard]] auto parse_framework(std::string_view s, Framework& out) -> bool;
[[nodiscard]] auto run_status_name(RunStatus s) -> std::string_view;
[[nodiscard]] auto endpoint_status_name(EndpointStatus s) -> std::string_view;
[[nodiscard]] auto tuning_strategy_name(TuningStrategy s) -> std::string_view;
[[nodiscard]] auto parse_tuning_strategy(std::string_view s, TuningStrategy& out) -> bool;

// ── MODEL (PRD §5) ──
struct ModelEntry {
    std::string name;
    ModelType type = ModelType::CLASSIFICATION;
    std::string description;
    std::string owner;
    std::string status = "REGISTERED";
    uint32_t latest_version = 0;       // 0 = no trained version yet
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

// ── MODEL_TEMPLATE (PRD §7) ──
struct ModelTemplateEntry {
    std::string name;
    Framework framework = Framework::SKLEARN;
    std::string algorithm;
    std::chrono::system_clock::time_point created_at{};
};

// ── TRAINING_JOB (PRD §8) ──
struct TrainingJobEntry {
    std::string name;
    std::string model;
    std::string feature_set;
    std::string dataset;
    Framework framework = Framework::SKLEARN;
    std::string algorithm;
    std::string entrypoint;
    std::string objective;             // LLM objective (PRD §10)
    std::map<std::string, std::string> hyperparams;
    std::chrono::system_clock::time_point created_at{};
};

// ── TUNING_JOB (PRD §13) ──
struct TuningJobEntry {
    std::string name;
    std::string model;
    std::string training_job;
    TuningStrategy strategy = TuningStrategy::OPTUNA;
    std::string objective;             // metric to optimize, e.g. ROC_AUC
    uint32_t trials = 20;
    // search space: param -> spec string (e.g. "10..200" or "0.01,0.1,0.3")
    std::map<std::string, std::string> search_space;
    std::chrono::system_clock::time_point created_at{};
};

// ── MODEL_RUN (PRD §11) ──
struct ModelRunEntry {
    std::string run_id;
    std::string model;
    std::string training_job;
    std::string tuning_job;            // empty unless part of a tuning sweep
    uint32_t feature_set_version = 0;
    uint32_t dataset_version = 0;
    std::map<std::string, std::string> hyperparameters;
    std::map<std::string, double> metrics;
    std::string artifact_location;
    RunStatus status = RunStatus::QUEUED;
    std::string error;
    std::chrono::system_clock::time_point start_time{};
    std::chrono::system_clock::time_point end_time{};
};

// ── MODEL_VERSION (PRD §12) ──
struct ModelVersionEntry {
    std::string model;
    uint32_t version = 1;
    std::string run_id;
    std::string artifact_location;
    std::map<std::string, double> metrics;
    std::chrono::system_clock::time_point created_at{};
};

// ── MODEL_ENDPOINT (PRD §17) ──
struct ModelEndpointEntry {
    std::string name;                  // endpoint name (default model_vN)
    std::string model;
    uint32_t version = 1;
    EndpointStatus status = EndpointStatus::ACTIVE;
    std::string artifact_location;
    // monitoring counters (PRD §21)
    uint64_t prediction_count = 0;
    uint64_t failure_count = 0;
    double total_latency_ms = 0.0;
    std::chrono::system_clock::time_point created_at{};
};

// ── A single stored prediction (feeds PREDICTION_TABLE + monitoring) ──
struct PredictionRecord {
    std::string entity_key;
    std::string prediction;
    double confidence = 0.0;
    std::string model_version;
    std::chrono::system_clock::time_point timestamp{};
};

} // namespace mnemo::models
