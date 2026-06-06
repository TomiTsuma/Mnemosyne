// src/Models/model_catalog.cpp — enum <-> string helpers

#include "Models/model_catalog.h"
#include <algorithm>
#include <cctype>

namespace mnemo::models {

namespace {
auto upper(std::string_view s) -> std::string {
    std::string out{s};
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}
} // namespace

auto model_type_name(ModelType t) -> std::string_view {
    switch (t) {
        case ModelType::CLASSIFICATION: return "CLASSIFICATION";
        case ModelType::REGRESSION:     return "REGRESSION";
        case ModelType::FORECASTING:    return "FORECASTING";
        case ModelType::RECOMMENDATION: return "RECOMMENDATION";
        case ModelType::CLUSTERING:     return "CLUSTERING";
        case ModelType::EMBEDDING:      return "EMBEDDING";
        case ModelType::LLM:            return "LLM";
        case ModelType::RL:             return "RL";
        case ModelType::CUSTOM:         return "CUSTOM";
    }
    return "CUSTOM";
}

auto parse_model_type(std::string_view s, ModelType& out) -> bool {
    const auto u = upper(s);
    if (u == "CLASSIFICATION") { out = ModelType::CLASSIFICATION; return true; }
    if (u == "REGRESSION")     { out = ModelType::REGRESSION; return true; }
    if (u == "FORECASTING")    { out = ModelType::FORECASTING; return true; }
    if (u == "RECOMMENDATION") { out = ModelType::RECOMMENDATION; return true; }
    if (u == "CLUSTERING")     { out = ModelType::CLUSTERING; return true; }
    if (u == "EMBEDDING")      { out = ModelType::EMBEDDING; return true; }
    if (u == "LLM")            { out = ModelType::LLM; return true; }
    if (u == "RL")             { out = ModelType::RL; return true; }
    if (u == "CUSTOM")         { out = ModelType::CUSTOM; return true; }
    return false;
}

auto framework_name(Framework f) -> std::string_view {
    switch (f) {
        case Framework::SKLEARN:    return "SKLEARN";
        case Framework::XGBOOST:    return "XGBOOST";
        case Framework::LIGHTGBM:   return "LIGHTGBM";
        case Framework::CATBOOST:   return "CATBOOST";
        case Framework::PYTORCH:    return "PYTORCH";
        case Framework::TENSORFLOW: return "TENSORFLOW";
        case Framework::CUSTOM:     return "CUSTOM";
    }
    return "SKLEARN";
}

auto parse_framework(std::string_view s, Framework& out) -> bool {
    const auto u = upper(s);
    if (u == "SKLEARN" || u == "SCIKIT-LEARN" || u == "SCIKIT_LEARN") { out = Framework::SKLEARN; return true; }
    if (u == "XGBOOST")    { out = Framework::XGBOOST; return true; }
    if (u == "LIGHTGBM")   { out = Framework::LIGHTGBM; return true; }
    if (u == "CATBOOST")   { out = Framework::CATBOOST; return true; }
    if (u == "PYTORCH" || u == "TORCH") { out = Framework::PYTORCH; return true; }
    if (u == "TENSORFLOW" || u == "TF") { out = Framework::TENSORFLOW; return true; }
    if (u == "CUSTOM")     { out = Framework::CUSTOM; return true; }
    return false;
}

auto run_status_name(RunStatus s) -> std::string_view {
    switch (s) {
        case RunStatus::QUEUED:    return "QUEUED";
        case RunStatus::RUNNING:   return "RUNNING";
        case RunStatus::SUCCEEDED: return "SUCCEEDED";
        case RunStatus::FAILED:    return "FAILED";
        case RunStatus::CANCELLED: return "CANCELLED";
    }
    return "QUEUED";
}

auto endpoint_status_name(EndpointStatus s) -> std::string_view {
    switch (s) {
        case EndpointStatus::STARTING: return "STARTING";
        case EndpointStatus::ACTIVE:   return "ACTIVE";
        case EndpointStatus::SCALING:  return "SCALING";
        case EndpointStatus::FAILED:   return "FAILED";
        case EndpointStatus::STOPPED:  return "STOPPED";
    }
    return "ACTIVE";
}

auto tuning_strategy_name(TuningStrategy s) -> std::string_view {
    switch (s) {
        case TuningStrategy::GRID:         return "GRID";
        case TuningStrategy::RANDOM:       return "RANDOM";
        case TuningStrategy::OPTUNA:       return "OPTUNA";
        case TuningStrategy::HYPEROPT:     return "HYPEROPT";
        case TuningStrategy::BAYESIAN:     return "BAYESIAN";
        case TuningStrategy::EVOLUTIONARY: return "EVOLUTIONARY";
    }
    return "OPTUNA";
}

auto parse_tuning_strategy(std::string_view s, TuningStrategy& out) -> bool {
    const auto u = upper(s);
    if (u == "GRID")         { out = TuningStrategy::GRID; return true; }
    if (u == "RANDOM")       { out = TuningStrategy::RANDOM; return true; }
    if (u == "OPTUNA")       { out = TuningStrategy::OPTUNA; return true; }
    if (u == "HYPEROPT")     { out = TuningStrategy::HYPEROPT; return true; }
    if (u == "BAYESIAN")     { out = TuningStrategy::BAYESIAN; return true; }
    if (u == "EVOLUTIONARY") { out = TuningStrategy::EVOLUTIONARY; return true; }
    return false;
}

} // namespace mnemo::models
