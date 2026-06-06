// src/Interpreters/interpreter_model_control.cpp — MODEL layer control verbs

#include "Interpreters/interpreter_model_control.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Models/model_manager.h"
#include "Models/model_catalog.h"
#include "Models/model_runtime.h"
#include "FeatureSets/feature_set_manager.h"
#include "FeatureSets/feature_set_catalog.h"
#include "FeatureSets/feature_set_resolver.h"
#include "Storages/memory_storage.h"
#include "Databases/database.h"
#include "DataTypes/data_type_factory.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"

#include <atomic>
#include <chrono>
#include <map>

namespace mnemo::interpreters {

namespace {

auto now() -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::now();
}

auto gen_run_id(const std::string& model) -> std::string {
    static std::atomic<uint64_t> counter{0};
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    return model + "_" + std::to_string(ms) + "_" + std::to_string(counter.fetch_add(1));
}

// Resolve the training data Block for a feature set declared on a job.
auto resolve_feature_block(Context& context, const std::string& feature_set_name)
    -> core::Block {
    const auto* fs = feature_sets::FeatureSetManager::instance().get_feature_set(feature_set_name);
    if (!fs) {
        throw common::Exception{
            "Unknown feature set: " + feature_set_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    const std::string source = fs->source_table.empty() ? fs->name : fs->source_table;
    auto storage = ddl_utils::resolve_storage(context, source);
    if (!storage) {
        throw common::Exception{
            "Feature set '" + feature_set_name + "' source table '" + source + "' not found",
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return feature_sets::resolve_block(*storage, *fs);
}

auto single_value_block(const std::string& col, const std::string& value) -> core::Block {
    auto c = std::make_shared<columns::ColumnString>();
    c->insert(core::Field(value));
    core::Block b;
    b.add_column(col, c);
    return b;
}

auto metrics_block(const std::map<std::string, double>& metrics) -> core::Block {
    auto k = std::make_shared<columns::ColumnString>();
    auto v = std::make_shared<columns::ColumnString>();
    for (const auto& [key, val] : metrics) {
        k->insert(core::Field(key));
        v->insert(core::Field(std::to_string(val)));
    }
    core::Block b;
    b.add_column("metric", k);
    b.add_column("value", v);
    return b;
}

} // namespace

auto InterpreterModelControl::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    using Action = parsers::QueryAST::ModelControl::Action;
    switch (query.model_control.action) {
        case Action::RunTrainingJob: return run_training_job(context, query);
        case Action::RunTuningJob:   return run_tuning_job(context, query);
        case Action::Deploy:         return deploy(context, query);
        case Action::Predict:        return predict(context, query);
        case Action::Evaluate:       return evaluate(context, query);
        case Action::Compare:        return compare(context, query);
        case Action::Generate:       return generate(context, query);
        default:
            throw common::Exception{
                "Unsupported model control action",
                static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
    }
}

auto InterpreterModelControl::run_training_job(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    auto& mm = models::ModelManager::instance();
    const auto& job_name = query.model_control.target_name;
    auto job = mm.get_training_job(job_name);
    if (!job) {
        throw common::Exception{"Unknown training job: " + job_name,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    auto model = mm.get_model(job->model);
    if (!model) {
        throw common::Exception{"Training job references unknown model: " + job->model,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    models::RuntimeRequest req;
    req.action = "train";
    req.run_id = gen_run_id(job->model);
    req.model = job->model;
    req.model_type = std::string(models::model_type_name(model->type));
    req.framework = std::string(models::framework_name(job->framework));
    req.algorithm = job->algorithm;
    req.entrypoint = job->entrypoint;
    req.objective = job->objective;
    req.hyperparameters = job->hyperparams;

    core::Block data;
    if (!job->feature_set.empty()) {
        const auto* fs = feature_sets::FeatureSetManager::instance().get_feature_set(job->feature_set);
        if (!fs) {
            throw common::Exception{"Unknown feature set: " + job->feature_set,
                                    static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        req.entity_key = fs->entity_key;
        req.features = fs->features;
        req.target = fs->target;
        data = resolve_feature_block(context, job->feature_set);
        req.data = &data;
    }

    models::ModelRunEntry run;
    run.run_id = req.run_id;
    run.model = job->model;
    run.training_job = job_name;
    run.hyperparameters = job->hyperparams;
    run.status = models::RunStatus::RUNNING;
    run.start_time = now();
    mm.add_run(run);

    auto res = models::ModelRuntime::run(req);
    run.end_time = now();
    if (!res.ok) {
        run.status = models::RunStatus::FAILED;
        run.error = res.error;
        mm.update_run(run);
        throw common::Exception{"Training failed: " + res.error,
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    run.status = models::RunStatus::SUCCEEDED;
    run.metrics = res.metrics;
    run.artifact_location = res.artifact_location;
    mm.update_run(run);

    const uint32_t version = mm.register_version(job->model, run.run_id,
                                                 res.artifact_location, res.metrics);

    auto run_col = std::make_shared<columns::ColumnString>();
    auto ver_col = std::make_shared<columns::ColumnString>();
    auto status_col = std::make_shared<columns::ColumnString>();
    auto metrics_col = std::make_shared<columns::ColumnString>();
    run_col->insert(core::Field(run.run_id));
    ver_col->insert(core::Field("v" + std::to_string(version)));
    status_col->insert(core::Field(std::string("SUCCEEDED")));
    std::string m;
    for (const auto& [k, v] : res.metrics) {
        if (!m.empty()) m += ", ";
        m += k + "=" + std::to_string(v);
    }
    metrics_col->insert(core::Field(m));
    core::Block b;
    b.add_column("run_id", run_col);
    b.add_column("version", ver_col);
    b.add_column("status", status_col);
    b.add_column("metrics", metrics_col);
    return b;
}

auto InterpreterModelControl::run_tuning_job(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    auto& mm = models::ModelManager::instance();
    const auto& job_name = query.model_control.target_name;
    auto tuning = mm.get_tuning_job(job_name);
    if (!tuning) {
        throw common::Exception{"Unknown tuning job: " + job_name,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    auto training = mm.get_training_job(tuning->training_job);
    if (!training) {
        throw common::Exception{"Tuning job references unknown training job: " + tuning->training_job,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    auto model = mm.get_model(tuning->model.empty() ? training->model : tuning->model);
    const std::string model_name = tuning->model.empty() ? training->model : tuning->model;
    if (!model) {
        throw common::Exception{"Tuning job references unknown model: " + model_name,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    models::RuntimeRequest req;
    req.action = "tune";
    req.run_id = gen_run_id(model_name);
    req.model = model_name;
    req.model_type = std::string(models::model_type_name(model->type));
    req.framework = std::string(models::framework_name(training->framework));
    req.algorithm = training->algorithm;
    req.objective = tuning->objective.empty() ? training->objective : tuning->objective;
    req.hyperparameters = training->hyperparams;
    req.strategy = std::string(models::tuning_strategy_name(tuning->strategy));
    req.trials = tuning->trials;
    req.search_space = tuning->search_space;

    core::Block data;
    if (!training->feature_set.empty()) {
        const auto* fs = feature_sets::FeatureSetManager::instance().get_feature_set(training->feature_set);
        if (!fs) {
            throw common::Exception{"Unknown feature set: " + training->feature_set,
                                    static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        req.entity_key = fs->entity_key;
        req.features = fs->features;
        req.target = fs->target;
        data = resolve_feature_block(context, training->feature_set);
        req.data = &data;
    }

    auto res = models::ModelRuntime::run(req);
    if (!res.ok) {
        throw common::Exception{"Tuning failed: " + res.error,
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    // Record one MODEL_RUN per trial for traceability.
    for (size_t i = 0; i < res.trials.size(); ++i) {
        models::ModelRunEntry run;
        run.run_id = req.run_id + "_trial" + std::to_string(i);
        run.model = model_name;
        run.training_job = tuning->training_job;
        run.tuning_job = job_name;
        run.hyperparameters = res.trials[i].params;
        run.metrics = res.trials[i].metrics;
        run.status = models::RunStatus::SUCCEEDED;
        run.start_time = now();
        run.end_time = run.start_time;
        mm.add_run(run);
    }

    const std::string best_run_id = req.run_id + "_best";
    {
        models::ModelRunEntry run;
        run.run_id = best_run_id;
        run.model = model_name;
        run.training_job = tuning->training_job;
        run.tuning_job = job_name;
        run.hyperparameters = res.best_params;
        run.metrics = res.metrics;
        run.artifact_location = res.artifact_location;
        run.status = models::RunStatus::SUCCEEDED;
        run.start_time = now();
        run.end_time = run.start_time;
        mm.add_run(run);
    }
    const uint32_t version = mm.register_version(model_name, best_run_id,
                                                 res.artifact_location, res.metrics);

    auto trial_col = std::make_shared<columns::ColumnString>();
    auto params_col = std::make_shared<columns::ColumnString>();
    auto obj_col = std::make_shared<columns::ColumnString>();
    for (size_t i = 0; i < res.trials.size(); ++i) {
        trial_col->insert(core::Field(std::to_string(i)));
        std::string p;
        for (const auto& [k, v] : res.trials[i].params) {
            if (!p.empty()) p += ", ";
            p += k + "=" + v;
        }
        params_col->insert(core::Field(p));
        obj_col->insert(core::Field(std::to_string(res.trials[i].objective_value)));
    }
    // Append a summary "best" row.
    trial_col->insert(core::Field(std::string("best -> v") + std::to_string(version)));
    std::string bp;
    for (const auto& [k, v] : res.best_params) {
        if (!bp.empty()) bp += ", ";
        bp += k + "=" + v;
    }
    params_col->insert(core::Field(bp));
    obj_col->insert(core::Field(std::string("")));

    core::Block b;
    b.add_column("trial", trial_col);
    b.add_column("params", params_col);
    b.add_column("objective_value", obj_col);
    return b;
}

auto InterpreterModelControl::deploy(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mm = models::ModelManager::instance();
    const auto& mc = query.model_control;
    if (!mm.has_model(mc.model_name)) {
        throw common::Exception{"Unknown model: " + mc.model_name,
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    uint32_t version = mc.version != 0 ? mc.version : mm.latest_version(mc.model_name);
    if (version == 0) {
        throw common::Exception{"Model '" + mc.model_name + "' has no trained version to deploy",
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    auto ep = mm.deploy(mc.model_name, version, mc.endpoint_name);

    auto ep_col = std::make_shared<columns::ColumnString>();
    auto model_col = std::make_shared<columns::ColumnString>();
    auto ver_col = std::make_shared<columns::ColumnString>();
    auto status_col = std::make_shared<columns::ColumnString>();
    ep_col->insert(core::Field(ep.name));
    model_col->insert(core::Field(ep.model));
    ver_col->insert(core::Field("v" + std::to_string(ep.version)));
    status_col->insert(core::Field(std::string(models::endpoint_status_name(ep.status))));
    core::Block b;
    b.add_column("endpoint", ep_col);
    b.add_column("model", model_col);
    b.add_column("version", ver_col);
    b.add_column("status", status_col);
    return b;
}

auto InterpreterModelControl::predict(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    auto& mm = models::ModelManager::instance();
    const auto& mc = query.model_control;
    uint32_t version = mc.version != 0 ? mc.version : mm.latest_version(mc.model_name);
    if (version == 0) {
        throw common::Exception{"Model '" + mc.model_name + "' has no trained version",
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    auto ver = mm.get_version(mc.model_name, version);
    if (!ver) {
        throw common::Exception{"Unknown model version v" + std::to_string(version),
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    models::RuntimeRequest req;
    req.action = "predict";
    req.run_id = gen_run_id(mc.model_name) + "_predict";
    req.model = mc.model_name;
    req.artifact_in = ver->artifact_location;
    req.predict_mode = mc.predict_mode.empty() ? "features" : mc.predict_mode;
    for (const auto& [k, v] : mc.kv) req.predict_features[k] = v;

    core::Block data;
    if (mc.predict_mode == "batch") {
        auto storage = ddl_utils::resolve_storage(context, mc.from_table);
        if (!storage) {
            throw common::Exception{"Unknown table: " + mc.from_table,
                                    static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        data = storage->read(storage->columns());
        req.data = &data;
    } else if (mc.predict_mode == "entity") {
        // Entity lookup needs the source data: chain the trained version back to
        // its training job's feature set so the runtime can find the entity row.
        if (auto run = mm.get_run(ver->run_id)) {
            if (auto job = mm.get_training_job(run->training_job);
                job && !job->feature_set.empty()) {
                if (const auto* fs = feature_sets::FeatureSetManager::instance()
                                         .get_feature_set(job->feature_set)) {
                    req.entity_key = fs->entity_key;
                    req.features = fs->features;
                    req.target = fs->target;
                    data = resolve_feature_block(context, job->feature_set);
                    req.data = &data;
                }
            }
        }
    }

    const auto start = std::chrono::steady_clock::now();
    auto res = models::ModelRuntime::run(req);
    const double latency_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - start)
                                  .count();
    if (!res.ok) {
        models::PredictionRecord rec;
        rec.model_version = "v" + std::to_string(version);
        rec.timestamp = now();
        mm.record_prediction(mc.model_name, rec, latency_ms, /*failed=*/true);
        throw common::Exception{"Prediction failed: " + res.error,
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    for (const auto& p : res.predictions) {
        models::PredictionRecord rec;
        rec.entity_key = p.entity_key;
        rec.prediction = p.prediction;
        rec.confidence = p.confidence;
        rec.model_version = "v" + std::to_string(version);
        rec.timestamp = now();
        mm.record_prediction(mc.model_name, rec, latency_ms, /*failed=*/false);
    }

    // Build the result block.
    auto key_col = std::make_shared<columns::ColumnString>();
    auto pred_col = std::make_shared<columns::ColumnString>();
    auto conf_col = std::make_shared<columns::ColumnString>();
    auto ver_col = std::make_shared<columns::ColumnString>();
    for (const auto& p : res.predictions) {
        key_col->insert(core::Field(p.entity_key));
        pred_col->insert(core::Field(p.prediction));
        conf_col->insert(core::Field(std::to_string(p.confidence)));
        ver_col->insert(core::Field("v" + std::to_string(version)));
    }
    core::Block b;
    b.add_column("entity_key", key_col);
    b.add_column("prediction", pred_col);
    b.add_column("confidence", conf_col);
    b.add_column("model_version", ver_col);

    // For batch prediction, materialize a PREDICTION_TABLE (PRD §19).
    if (mc.predict_mode == "batch") {
        const std::string table_name = mc.model_name + "_predictions";
        const auto db_name = ddl_utils::resolve_current_database(context);
        if (auto db = context.get_database(db_name)) {
            std::unordered_map<std::string, datatypes::DataTypePtr> cols;
            auto str = datatypes::get_data_type("String");
            cols["entity_key"] = str;
            cols["prediction"] = str;
            cols["confidence"] = str;
            cols["model_version"] = str;
            std::shared_ptr<storages::IStorage> storage;
            if (db->table_exists(table_name)) {
                storage = db->table(table_name);
            } else {
                storage = db->create_table(table_name, cols, "Memory");
                if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
                    mem->set_columns(cols);
                }
            }
            if (storage) {
                core::Block out;
                out.add_column("entity_key", key_col);
                out.add_column("prediction", pred_col);
                out.add_column("confidence", conf_col);
                out.add_column("model_version", ver_col);
                storage->write(out);
                context.register_storage(table_name, storage);
            }
        }
    }
    return b;
}

auto InterpreterModelControl::evaluate(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    auto& mm = models::ModelManager::instance();
    const auto& mc = query.model_control;
    uint32_t version = mc.version != 0 ? mc.version : mm.latest_version(mc.model_name);
    if (version == 0) {
        throw common::Exception{"Model '" + mc.model_name + "' has no trained version",
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    auto ver = mm.get_version(mc.model_name, version);
    if (!ver) {
        throw common::Exception{"Unknown model version v" + std::to_string(version),
                                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    // Find a training job for this model to locate the feature set + algorithm.
    std::string feature_set;
    for (const auto& job : mm.list_training_jobs()) {
        if (job.model == mc.model_name) { feature_set = job.feature_set; break; }
    }

    models::RuntimeRequest req;
    req.action = "evaluate";
    req.run_id = gen_run_id(mc.model_name) + "_eval";
    req.model = mc.model_name;
    req.artifact_in = ver->artifact_location;

    core::Block data;
    if (!feature_set.empty()) {
        const auto* fs = feature_sets::FeatureSetManager::instance().get_feature_set(feature_set);
        if (fs) {
            req.entity_key = fs->entity_key;
            req.features = fs->features;
            req.target = fs->target;
            data = resolve_feature_block(context, feature_set);
            req.data = &data;
        }
    }

    auto res = models::ModelRuntime::run(req);
    if (!res.ok) {
        throw common::Exception{"Evaluation failed: " + res.error,
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return metrics_block(res.metrics);
}

auto InterpreterModelControl::compare(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mm = models::ModelManager::instance();
    const auto& mc = query.model_control;

    auto model_col = std::make_shared<columns::ColumnString>();
    auto ver_col = std::make_shared<columns::ColumnString>();
    auto metric_col = std::make_shared<columns::ColumnString>();
    auto value_col = std::make_shared<columns::ColumnString>();
    for (const auto& [model, ver_in] : mc.compare_targets) {
        uint32_t version = ver_in != 0 ? ver_in : mm.latest_version(model);
        auto ver = mm.get_version(model, version);
        if (!ver) {
            model_col->insert(core::Field(model));
            ver_col->insert(core::Field("v" + std::to_string(version)));
            metric_col->insert(core::Field(std::string("(none)")));
            value_col->insert(core::Field(std::string("not found")));
            continue;
        }
        if (ver->metrics.empty()) {
            model_col->insert(core::Field(model));
            ver_col->insert(core::Field("v" + std::to_string(version)));
            metric_col->insert(core::Field(std::string("(none)")));
            value_col->insert(core::Field(std::string("")));
        }
        for (const auto& [k, v] : ver->metrics) {
            model_col->insert(core::Field(model));
            ver_col->insert(core::Field("v" + std::to_string(version)));
            metric_col->insert(core::Field(k));
            value_col->insert(core::Field(std::to_string(v)));
        }
    }
    core::Block b;
    b.add_column("model", model_col);
    b.add_column("version", ver_col);
    b.add_column("metric", metric_col);
    b.add_column("value", value_col);
    return b;
}

auto InterpreterModelControl::generate(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mm = models::ModelManager::instance();
    const auto& mc = query.model_control;
    auto model = mm.get_model(mc.model_name);

    models::RuntimeRequest req;
    req.action = "generate";
    req.run_id = gen_run_id(mc.model_name) + "_gen";
    req.model = mc.model_name;
    req.model_type = model ? std::string(models::model_type_name(model->type)) : std::string("LLM");
    req.prompt = mc.prompt;

    auto res = models::ModelRuntime::run(req);
    if (!res.ok) {
        throw common::Exception{"Generation failed: " + res.error,
                                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return single_value_block("generated_text", res.generated_text);
}

} // namespace mnemo::interpreters
