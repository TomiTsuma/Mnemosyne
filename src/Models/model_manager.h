// src/Models/model_manager.h — MODEL layer registry + persistence
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Models/model_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::models {

// Resolve the models data directory (env MNEMO_MODELS_DIR, default ./mnemo_models).
[[nodiscard]] auto models_dir() -> std::string;

class ModelManager {
public:
    static auto instance() -> ModelManager&;

    // ── MODEL ──
    auto create_model(ModelEntry entry, bool if_not_exists) -> void;
    auto drop_model(const std::string& name, bool if_exists) -> void;
    [[nodiscard]] auto get_model(std::string_view name) const -> std::optional<ModelEntry>;
    [[nodiscard]] auto has_model(std::string_view name) const -> bool;
    [[nodiscard]] auto list_models() const -> std::vector<ModelEntry>;

    // ── MODEL_TEMPLATE ──
    auto create_template(ModelTemplateEntry entry, bool if_not_exists) -> void;
    auto drop_template(const std::string& name, bool if_exists) -> void;
    [[nodiscard]] auto get_template(std::string_view name) const -> std::optional<ModelTemplateEntry>;
    [[nodiscard]] auto list_templates() const -> std::vector<ModelTemplateEntry>;

    // ── TRAINING_JOB ──
    auto create_training_job(TrainingJobEntry entry, bool if_not_exists) -> void;
    auto drop_training_job(const std::string& name, bool if_exists) -> void;
    [[nodiscard]] auto get_training_job(std::string_view name) const -> std::optional<TrainingJobEntry>;
    [[nodiscard]] auto list_training_jobs() const -> std::vector<TrainingJobEntry>;

    // ── TUNING_JOB ──
    auto create_tuning_job(TuningJobEntry entry, bool if_not_exists) -> void;
    auto drop_tuning_job(const std::string& name, bool if_exists) -> void;
    [[nodiscard]] auto get_tuning_job(std::string_view name) const -> std::optional<TuningJobEntry>;
    [[nodiscard]] auto list_tuning_jobs() const -> std::vector<TuningJobEntry>;

    // ── MODEL_RUN ──
    auto add_run(ModelRunEntry run) -> void;
    auto update_run(const ModelRunEntry& run) -> void;
    [[nodiscard]] auto get_run(std::string_view run_id) const -> std::optional<ModelRunEntry>;
    [[nodiscard]] auto list_runs() const -> std::vector<ModelRunEntry>;
    [[nodiscard]] auto list_runs_for_model(std::string_view model) const -> std::vector<ModelRunEntry>;

    // ── MODEL_VERSION ──
    // Register a new version from a successful run; returns the assigned version number.
    auto register_version(const std::string& model, const std::string& run_id,
                          const std::string& artifact_location,
                          const std::map<std::string, double>& metrics) -> uint32_t;
    [[nodiscard]] auto list_versions(std::string_view model) const -> std::vector<ModelVersionEntry>;
    [[nodiscard]] auto get_version(std::string_view model, uint32_t version) const
        -> std::optional<ModelVersionEntry>;
    [[nodiscard]] auto latest_version(std::string_view model) const -> uint32_t;

    // ── MODEL_ENDPOINT ──
    auto deploy(const std::string& model, uint32_t version, const std::string& endpoint_name)
        -> ModelEndpointEntry;
    auto undeploy(const std::string& endpoint_name, bool if_exists) -> void;
    [[nodiscard]] auto get_endpoint(std::string_view name) const -> std::optional<ModelEndpointEntry>;
    [[nodiscard]] auto find_active_endpoint(std::string_view model) const
        -> std::optional<ModelEndpointEntry>;
    [[nodiscard]] auto list_endpoints() const -> std::vector<ModelEndpointEntry>;

    // ── Monitoring (PRD §21) ──
    auto record_prediction(const std::string& model, const PredictionRecord& rec,
                           double latency_ms, bool failed) -> void;
    [[nodiscard]] auto list_predictions(std::string_view model) const -> std::vector<PredictionRecord>;

    // ── Persistence ──
    auto save() const -> void;
    auto load() -> void;

private:
    ModelManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ModelEntry> models_;
    std::unordered_map<std::string, ModelTemplateEntry> templates_;
    std::unordered_map<std::string, TrainingJobEntry> training_jobs_;
    std::unordered_map<std::string, TuningJobEntry> tuning_jobs_;
    std::unordered_map<std::string, ModelRunEntry> runs_;                 // by run_id
    std::unordered_map<std::string, std::vector<ModelVersionEntry>> versions_; // by model
    std::unordered_map<std::string, ModelEndpointEntry> endpoints_;       // by endpoint name
    std::unordered_map<std::string, std::vector<PredictionRecord>> predictions_; // by model

    auto save_locked() const -> void;
};

} // namespace mnemo::models
