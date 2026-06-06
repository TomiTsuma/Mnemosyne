// src/Models/model_manager.cpp

#include "Models/model_manager.h"
#include "Models/json.h"
#include "Common/exceptions.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mnemo::models {

namespace fs = std::filesystem;
using json::Value;
using json::Object;
using json::Array;

namespace {

auto to_epoch(std::chrono::system_clock::time_point tp) -> int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}
auto from_epoch(int64_t s) -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::time_point{std::chrono::seconds{s}};
}

auto string_map_to_json(const std::map<std::string, std::string>& m) -> Value {
    Object obj;
    for (const auto& [k, v] : m) obj.emplace_back(k, Value{v});
    return Value{obj};
}
auto json_to_string_map(const Value& v) -> std::map<std::string, std::string> {
    std::map<std::string, std::string> m;
    for (const auto& [k, val] : v.as_object()) m[k] = val.as_string();
    return m;
}
auto double_map_to_json(const std::map<std::string, double>& m) -> Value {
    Object obj;
    for (const auto& [k, v] : m) obj.emplace_back(k, Value{v});
    return Value{obj};
}
auto json_to_double_map(const Value& v) -> std::map<std::string, double> {
    std::map<std::string, double> m;
    for (const auto& [k, val] : v.as_object()) m[k] = val.as_number();
    return m;
}

[[noreturn]] void throw_unknown(const std::string& what, const std::string& name) {
    throw common::Exception{"Unknown " + what + ": " + name,
                            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
}
[[noreturn]] void throw_exists(const std::string& what, const std::string& name) {
    throw common::Exception{what + " already exists: " + name,
                            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

} // namespace

auto models_dir() -> std::string {
    if (const char* env = std::getenv("MNEMO_MODELS_DIR"); env && *env) return env;
    return "mnemo_models";
}

auto ModelManager::instance() -> ModelManager& {
    static ModelManager inst;
    return inst;
}

// ────────────────────────────── MODEL ──────────────────────────────
auto ModelManager::create_model(ModelEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (models_.contains(entry.name)) {
        if (if_not_exists) return;
        throw_exists("Model", entry.name);
    }
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    models_.emplace(entry.name, std::move(entry));
    save_locked();
}

auto ModelManager::drop_model(const std::string& name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    if (!models_.contains(name)) {
        if (if_exists) return;
        throw_unknown("model", name);
    }
    models_.erase(name);
    versions_.erase(name);
    predictions_.erase(name);
    // Drop endpoints belonging to this model.
    for (auto it = endpoints_.begin(); it != endpoints_.end();) {
        if (it->second.model == name) it = endpoints_.erase(it); else ++it;
    }
    save_locked();
}

auto ModelManager::get_model(std::string_view name) const -> std::optional<ModelEntry> {
    std::lock_guard lock{mutex_};
    auto it = models_.find(std::string{name});
    if (it == models_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::has_model(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return models_.contains(std::string{name});
}

auto ModelManager::list_models() const -> std::vector<ModelEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ModelEntry> out;
    for (const auto& [_, e] : models_) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return out;
}

// ──────────────────────── MODEL_TEMPLATE ────────────────────────
auto ModelManager::create_template(ModelTemplateEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (templates_.contains(entry.name)) {
        if (if_not_exists) return;
        throw_exists("Model template", entry.name);
    }
    entry.created_at = std::chrono::system_clock::now();
    templates_.emplace(entry.name, std::move(entry));
    save_locked();
}

auto ModelManager::drop_template(const std::string& name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    if (!templates_.contains(name)) {
        if (if_exists) return;
        throw_unknown("model template", name);
    }
    templates_.erase(name);
    save_locked();
}

auto ModelManager::get_template(std::string_view name) const -> std::optional<ModelTemplateEntry> {
    std::lock_guard lock{mutex_};
    auto it = templates_.find(std::string{name});
    if (it == templates_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::list_templates() const -> std::vector<ModelTemplateEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ModelTemplateEntry> out;
    for (const auto& [_, e] : templates_) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return out;
}

// ──────────────────────── TRAINING_JOB ────────────────────────
auto ModelManager::create_training_job(TrainingJobEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (training_jobs_.contains(entry.name)) {
        if (if_not_exists) return;
        throw_exists("Training job", entry.name);
    }
    entry.created_at = std::chrono::system_clock::now();
    training_jobs_.emplace(entry.name, std::move(entry));
    save_locked();
}

auto ModelManager::drop_training_job(const std::string& name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    if (!training_jobs_.contains(name)) {
        if (if_exists) return;
        throw_unknown("training job", name);
    }
    training_jobs_.erase(name);
    save_locked();
}

auto ModelManager::get_training_job(std::string_view name) const -> std::optional<TrainingJobEntry> {
    std::lock_guard lock{mutex_};
    auto it = training_jobs_.find(std::string{name});
    if (it == training_jobs_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::list_training_jobs() const -> std::vector<TrainingJobEntry> {
    std::lock_guard lock{mutex_};
    std::vector<TrainingJobEntry> out;
    for (const auto& [_, e] : training_jobs_) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return out;
}

// ──────────────────────── TUNING_JOB ────────────────────────
auto ModelManager::create_tuning_job(TuningJobEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    if (tuning_jobs_.contains(entry.name)) {
        if (if_not_exists) return;
        throw_exists("Tuning job", entry.name);
    }
    entry.created_at = std::chrono::system_clock::now();
    tuning_jobs_.emplace(entry.name, std::move(entry));
    save_locked();
}

auto ModelManager::drop_tuning_job(const std::string& name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    if (!tuning_jobs_.contains(name)) {
        if (if_exists) return;
        throw_unknown("tuning job", name);
    }
    tuning_jobs_.erase(name);
    save_locked();
}

auto ModelManager::get_tuning_job(std::string_view name) const -> std::optional<TuningJobEntry> {
    std::lock_guard lock{mutex_};
    auto it = tuning_jobs_.find(std::string{name});
    if (it == tuning_jobs_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::list_tuning_jobs() const -> std::vector<TuningJobEntry> {
    std::lock_guard lock{mutex_};
    std::vector<TuningJobEntry> out;
    for (const auto& [_, e] : tuning_jobs_) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return out;
}

// ──────────────────────────── MODEL_RUN ────────────────────────────
auto ModelManager::add_run(ModelRunEntry run) -> void {
    std::lock_guard lock{mutex_};
    runs_[run.run_id] = std::move(run);
    save_locked();
}

auto ModelManager::update_run(const ModelRunEntry& run) -> void {
    std::lock_guard lock{mutex_};
    runs_[run.run_id] = run;
    save_locked();
}

auto ModelManager::get_run(std::string_view run_id) const -> std::optional<ModelRunEntry> {
    std::lock_guard lock{mutex_};
    auto it = runs_.find(std::string{run_id});
    if (it == runs_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::list_runs() const -> std::vector<ModelRunEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ModelRunEntry> out;
    for (const auto& [_, e] : runs_) out.push_back(e);
    std::sort(out.begin(), out.end(),
              [](auto& a, auto& b) { return a.start_time < b.start_time; });
    return out;
}

auto ModelManager::list_runs_for_model(std::string_view model) const -> std::vector<ModelRunEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ModelRunEntry> out;
    for (const auto& [_, e] : runs_) if (e.model == model) out.push_back(e);
    std::sort(out.begin(), out.end(),
              [](auto& a, auto& b) { return a.start_time < b.start_time; });
    return out;
}

// ──────────────────────────── MODEL_VERSION ────────────────────────────
auto ModelManager::register_version(const std::string& model, const std::string& run_id,
                                     const std::string& artifact_location,
                                     const std::map<std::string, double>& metrics) -> uint32_t {
    std::lock_guard lock{mutex_};
    auto& vec = versions_[model];
    uint32_t next = 1;
    for (const auto& v : vec) next = std::max(next, v.version + 1);
    ModelVersionEntry ver;
    ver.model = model;
    ver.version = next;
    ver.run_id = run_id;
    ver.artifact_location = artifact_location;
    ver.metrics = metrics;
    ver.created_at = std::chrono::system_clock::now();
    vec.push_back(ver);
    if (auto it = models_.find(model); it != models_.end()) {
        it->second.latest_version = next;
        it->second.status = "TRAINED";
        it->second.updated_at = ver.created_at;
    }
    save_locked();
    return next;
}

auto ModelManager::list_versions(std::string_view model) const -> std::vector<ModelVersionEntry> {
    std::lock_guard lock{mutex_};
    auto it = versions_.find(std::string{model});
    if (it == versions_.end()) return {};
    auto out = it->second;
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.version < b.version; });
    return out;
}

auto ModelManager::get_version(std::string_view model, uint32_t version) const
    -> std::optional<ModelVersionEntry> {
    std::lock_guard lock{mutex_};
    auto it = versions_.find(std::string{model});
    if (it == versions_.end()) return std::nullopt;
    for (const auto& v : it->second) if (v.version == version) return v;
    return std::nullopt;
}

auto ModelManager::latest_version(std::string_view model) const -> uint32_t {
    std::lock_guard lock{mutex_};
    auto it = versions_.find(std::string{model});
    if (it == versions_.end()) return 0;
    uint32_t latest = 0;
    for (const auto& v : it->second) latest = std::max(latest, v.version);
    return latest;
}

// ──────────────────────────── MODEL_ENDPOINT ────────────────────────────
auto ModelManager::deploy(const std::string& model, uint32_t version,
                          const std::string& endpoint_name) -> ModelEndpointEntry {
    std::lock_guard lock{mutex_};
    std::string artifact;
    if (auto vit = versions_.find(model); vit != versions_.end()) {
        for (const auto& v : vit->second)
            if (v.version == version) artifact = v.artifact_location;
    }
    ModelEndpointEntry ep;
    ep.name = endpoint_name.empty()
                  ? model + "_v" + std::to_string(version)
                  : endpoint_name;
    ep.model = model;
    ep.version = version;
    ep.status = EndpointStatus::ACTIVE;
    ep.artifact_location = artifact;
    ep.created_at = std::chrono::system_clock::now();
    endpoints_[ep.name] = ep;
    save_locked();
    return ep;
}

auto ModelManager::undeploy(const std::string& endpoint_name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    if (!endpoints_.contains(endpoint_name)) {
        if (if_exists) return;
        throw_unknown("model endpoint", endpoint_name);
    }
    endpoints_.erase(endpoint_name);
    save_locked();
}

auto ModelManager::get_endpoint(std::string_view name) const -> std::optional<ModelEndpointEntry> {
    std::lock_guard lock{mutex_};
    auto it = endpoints_.find(std::string{name});
    if (it == endpoints_.end()) return std::nullopt;
    return it->second;
}

auto ModelManager::find_active_endpoint(std::string_view model) const
    -> std::optional<ModelEndpointEntry> {
    std::lock_guard lock{mutex_};
    std::optional<ModelEndpointEntry> best;
    for (const auto& [_, ep] : endpoints_) {
        if (ep.model != model) continue;
        if (ep.status != EndpointStatus::ACTIVE) continue;
        if (!best || ep.version > best->version) best = ep;
    }
    return best;
}

auto ModelManager::list_endpoints() const -> std::vector<ModelEndpointEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ModelEndpointEntry> out;
    for (const auto& [_, e] : endpoints_) out.push_back(e);
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.name < b.name; });
    return out;
}

// ──────────────────────────── Monitoring ────────────────────────────
auto ModelManager::record_prediction(const std::string& model, const PredictionRecord& rec,
                                      double latency_ms, bool failed) -> void {
    std::lock_guard lock{mutex_};
    predictions_[model].push_back(rec);
    // Attribute to the active endpoint for this model, if any.
    for (auto& [_, ep] : endpoints_) {
        if (ep.model != model || ep.status != EndpointStatus::ACTIVE) continue;
        ep.prediction_count += 1;
        ep.total_latency_ms += latency_ms;
        if (failed) ep.failure_count += 1;
        break;
    }
    save_locked();
}

auto ModelManager::list_predictions(std::string_view model) const -> std::vector<PredictionRecord> {
    std::lock_guard lock{mutex_};
    auto it = predictions_.find(std::string{model});
    if (it == predictions_.end()) return {};
    return it->second;
}

// ──────────────────────────── Persistence ────────────────────────────
auto ModelManager::save() const -> void {
    std::lock_guard lock{mutex_};
    save_locked();
}

auto ModelManager::save_locked() const -> void {
    Object root;

    Array models;
    for (const auto& [_, m] : models_) {
        Object o;
        o.emplace_back("name", Value{m.name});
        o.emplace_back("type", Value{std::string{model_type_name(m.type)}});
        o.emplace_back("description", Value{m.description});
        o.emplace_back("owner", Value{m.owner});
        o.emplace_back("status", Value{m.status});
        o.emplace_back("latest_version", Value{static_cast<int64_t>(m.latest_version)});
        o.emplace_back("created_at", Value{to_epoch(m.created_at)});
        o.emplace_back("updated_at", Value{to_epoch(m.updated_at)});
        models.emplace_back(o);
    }
    root.emplace_back("models", Value{models});

    Array templates;
    for (const auto& [_, t] : templates_) {
        Object o;
        o.emplace_back("name", Value{t.name});
        o.emplace_back("framework", Value{std::string{framework_name(t.framework)}});
        o.emplace_back("algorithm", Value{t.algorithm});
        o.emplace_back("created_at", Value{to_epoch(t.created_at)});
        templates.emplace_back(o);
    }
    root.emplace_back("templates", Value{templates});

    Array tjobs;
    for (const auto& [_, j] : training_jobs_) {
        Object o;
        o.emplace_back("name", Value{j.name});
        o.emplace_back("model", Value{j.model});
        o.emplace_back("feature_set", Value{j.feature_set});
        o.emplace_back("dataset", Value{j.dataset});
        o.emplace_back("framework", Value{std::string{framework_name(j.framework)}});
        o.emplace_back("algorithm", Value{j.algorithm});
        o.emplace_back("entrypoint", Value{j.entrypoint});
        o.emplace_back("objective", Value{j.objective});
        o.emplace_back("hyperparams", string_map_to_json(j.hyperparams));
        o.emplace_back("created_at", Value{to_epoch(j.created_at)});
        tjobs.emplace_back(o);
    }
    root.emplace_back("training_jobs", Value{tjobs});

    Array ujobs;
    for (const auto& [_, j] : tuning_jobs_) {
        Object o;
        o.emplace_back("name", Value{j.name});
        o.emplace_back("model", Value{j.model});
        o.emplace_back("training_job", Value{j.training_job});
        o.emplace_back("strategy", Value{std::string{tuning_strategy_name(j.strategy)}});
        o.emplace_back("objective", Value{j.objective});
        o.emplace_back("trials", Value{static_cast<int64_t>(j.trials)});
        o.emplace_back("search_space", string_map_to_json(j.search_space));
        o.emplace_back("created_at", Value{to_epoch(j.created_at)});
        ujobs.emplace_back(o);
    }
    root.emplace_back("tuning_jobs", Value{ujobs});

    Array runs;
    for (const auto& [_, r] : runs_) {
        Object o;
        o.emplace_back("run_id", Value{r.run_id});
        o.emplace_back("model", Value{r.model});
        o.emplace_back("training_job", Value{r.training_job});
        o.emplace_back("tuning_job", Value{r.tuning_job});
        o.emplace_back("feature_set_version", Value{static_cast<int64_t>(r.feature_set_version)});
        o.emplace_back("dataset_version", Value{static_cast<int64_t>(r.dataset_version)});
        o.emplace_back("hyperparameters", string_map_to_json(r.hyperparameters));
        o.emplace_back("metrics", double_map_to_json(r.metrics));
        o.emplace_back("artifact_location", Value{r.artifact_location});
        o.emplace_back("status", Value{std::string{run_status_name(r.status)}});
        o.emplace_back("error", Value{r.error});
        o.emplace_back("start_time", Value{to_epoch(r.start_time)});
        o.emplace_back("end_time", Value{to_epoch(r.end_time)});
        runs.emplace_back(o);
    }
    root.emplace_back("runs", Value{runs});

    Array versions;
    for (const auto& [_, vec] : versions_) {
        for (const auto& v : vec) {
            Object o;
            o.emplace_back("model", Value{v.model});
            o.emplace_back("version", Value{static_cast<int64_t>(v.version)});
            o.emplace_back("run_id", Value{v.run_id});
            o.emplace_back("artifact_location", Value{v.artifact_location});
            o.emplace_back("metrics", double_map_to_json(v.metrics));
            o.emplace_back("created_at", Value{to_epoch(v.created_at)});
            versions.emplace_back(o);
        }
    }
    root.emplace_back("versions", Value{versions});

    Array endpoints;
    for (const auto& [_, e] : endpoints_) {
        Object o;
        o.emplace_back("name", Value{e.name});
        o.emplace_back("model", Value{e.model});
        o.emplace_back("version", Value{static_cast<int64_t>(e.version)});
        o.emplace_back("status", Value{std::string{endpoint_status_name(e.status)}});
        o.emplace_back("artifact_location", Value{e.artifact_location});
        o.emplace_back("prediction_count", Value{static_cast<int64_t>(e.prediction_count)});
        o.emplace_back("failure_count", Value{static_cast<int64_t>(e.failure_count)});
        o.emplace_back("total_latency_ms", Value{e.total_latency_ms});
        o.emplace_back("created_at", Value{to_epoch(e.created_at)});
        endpoints.emplace_back(o);
    }
    root.emplace_back("endpoints", Value{endpoints});

    Array preds;
    for (const auto& [model, vec] : predictions_) {
        for (const auto& r : vec) {
            Object o;
            o.emplace_back("model", Value{model});
            o.emplace_back("entity_key", Value{r.entity_key});
            o.emplace_back("prediction", Value{r.prediction});
            o.emplace_back("confidence", Value{r.confidence});
            o.emplace_back("model_version", Value{r.model_version});
            o.emplace_back("timestamp", Value{to_epoch(r.timestamp)});
            preds.emplace_back(o);
        }
    }
    root.emplace_back("predictions", Value{preds});

    try {
        const auto dir = models_dir();
        fs::create_directories(dir);
        const auto path = fs::path{dir} / "catalog.json";
        std::ofstream out{path, std::ios::trunc};
        if (out) out << Value{root}.dump();
    } catch (...) {
        // Persistence is best-effort; never abort an operation because the
        // catalog could not be flushed.
    }
}

auto ModelManager::load() -> void {
    std::lock_guard lock{mutex_};
    const auto path = fs::path{models_dir()} / "catalog.json";
    std::error_code ec;
    if (!fs::exists(path, ec)) return;
    std::ifstream in{path};
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    Value root;
    try {
        root = Value::parse(ss.str());
    } catch (...) {
        return;
    }
    if (!root.is_object()) return;

    models_.clear(); templates_.clear(); training_jobs_.clear(); tuning_jobs_.clear();
    runs_.clear(); versions_.clear(); endpoints_.clear(); predictions_.clear();

    for (const auto& v : root["models"].as_array()) {
        ModelEntry m;
        m.name = v["name"].as_string();
        ModelType t{}; if (parse_model_type(v["type"].as_string(), t)) m.type = t;
        m.description = v["description"].as_string();
        m.owner = v["owner"].as_string();
        m.status = v["status"].as_string();
        m.latest_version = static_cast<uint32_t>(v["latest_version"].as_int());
        m.created_at = from_epoch(v["created_at"].as_int());
        m.updated_at = from_epoch(v["updated_at"].as_int());
        models_[m.name] = m;
    }
    for (const auto& v : root["templates"].as_array()) {
        ModelTemplateEntry t;
        t.name = v["name"].as_string();
        Framework fw{}; if (parse_framework(v["framework"].as_string(), fw)) t.framework = fw;
        t.algorithm = v["algorithm"].as_string();
        t.created_at = from_epoch(v["created_at"].as_int());
        templates_[t.name] = t;
    }
    for (const auto& v : root["training_jobs"].as_array()) {
        TrainingJobEntry j;
        j.name = v["name"].as_string();
        j.model = v["model"].as_string();
        j.feature_set = v["feature_set"].as_string();
        j.dataset = v["dataset"].as_string();
        Framework fw{}; if (parse_framework(v["framework"].as_string(), fw)) j.framework = fw;
        j.algorithm = v["algorithm"].as_string();
        j.entrypoint = v["entrypoint"].as_string();
        j.objective = v["objective"].as_string();
        j.hyperparams = json_to_string_map(v["hyperparams"]);
        j.created_at = from_epoch(v["created_at"].as_int());
        training_jobs_[j.name] = j;
    }
    for (const auto& v : root["tuning_jobs"].as_array()) {
        TuningJobEntry j;
        j.name = v["name"].as_string();
        j.model = v["model"].as_string();
        j.training_job = v["training_job"].as_string();
        TuningStrategy s{}; if (parse_tuning_strategy(v["strategy"].as_string(), s)) j.strategy = s;
        j.objective = v["objective"].as_string();
        j.trials = static_cast<uint32_t>(v["trials"].as_int());
        j.search_space = json_to_string_map(v["search_space"]);
        j.created_at = from_epoch(v["created_at"].as_int());
        tuning_jobs_[j.name] = j;
    }
    for (const auto& v : root["runs"].as_array()) {
        ModelRunEntry r;
        r.run_id = v["run_id"].as_string();
        r.model = v["model"].as_string();
        r.training_job = v["training_job"].as_string();
        r.tuning_job = v["tuning_job"].as_string();
        r.feature_set_version = static_cast<uint32_t>(v["feature_set_version"].as_int());
        r.dataset_version = static_cast<uint32_t>(v["dataset_version"].as_int());
        r.hyperparameters = json_to_string_map(v["hyperparameters"]);
        r.metrics = json_to_double_map(v["metrics"]);
        r.artifact_location = v["artifact_location"].as_string();
        const auto st = v["status"].as_string();
        if (st == "RUNNING") r.status = RunStatus::RUNNING;
        else if (st == "SUCCEEDED") r.status = RunStatus::SUCCEEDED;
        else if (st == "FAILED") r.status = RunStatus::FAILED;
        else if (st == "CANCELLED") r.status = RunStatus::CANCELLED;
        else r.status = RunStatus::QUEUED;
        r.error = v["error"].as_string();
        r.start_time = from_epoch(v["start_time"].as_int());
        r.end_time = from_epoch(v["end_time"].as_int());
        runs_[r.run_id] = r;
    }
    for (const auto& v : root["versions"].as_array()) {
        ModelVersionEntry ver;
        ver.model = v["model"].as_string();
        ver.version = static_cast<uint32_t>(v["version"].as_int());
        ver.run_id = v["run_id"].as_string();
        ver.artifact_location = v["artifact_location"].as_string();
        ver.metrics = json_to_double_map(v["metrics"]);
        ver.created_at = from_epoch(v["created_at"].as_int());
        versions_[ver.model].push_back(ver);
    }
    for (const auto& v : root["endpoints"].as_array()) {
        ModelEndpointEntry e;
        e.name = v["name"].as_string();
        e.model = v["model"].as_string();
        e.version = static_cast<uint32_t>(v["version"].as_int());
        const auto st = v["status"].as_string();
        if (st == "STARTING") e.status = EndpointStatus::STARTING;
        else if (st == "SCALING") e.status = EndpointStatus::SCALING;
        else if (st == "FAILED") e.status = EndpointStatus::FAILED;
        else if (st == "STOPPED") e.status = EndpointStatus::STOPPED;
        else e.status = EndpointStatus::ACTIVE;
        e.artifact_location = v["artifact_location"].as_string();
        e.prediction_count = static_cast<uint64_t>(v["prediction_count"].as_int());
        e.failure_count = static_cast<uint64_t>(v["failure_count"].as_int());
        e.total_latency_ms = v["total_latency_ms"].as_number();
        e.created_at = from_epoch(v["created_at"].as_int());
        endpoints_[e.name] = e;
    }
    for (const auto& v : root["predictions"].as_array()) {
        PredictionRecord r;
        const auto model = v["model"].as_string();
        r.entity_key = v["entity_key"].as_string();
        r.prediction = v["prediction"].as_string();
        r.confidence = v["confidence"].as_number();
        r.model_version = v["model_version"].as_string();
        r.timestamp = from_epoch(v["timestamp"].as_int());
        predictions_[model].push_back(r);
    }
}

} // namespace mnemo::models
