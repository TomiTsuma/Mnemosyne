// src/Models/model_runtime.cpp

#include "Models/model_runtime.h"
#include "Models/model_manager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mnemo::models {

namespace fs = std::filesystem;
using json::Value;
using json::Object;
using json::Array;

auto field_to_string(const core::Field& f) -> std::string {
    if (f.is_null()) return "";
    if (auto s = f.as_string()) return *s;
    // NOTE: as_bool() interprets ANY non-zero integer as true, so it must only
    // be used for genuine Bool fields — otherwise integer columns like ids would
    // all collapse to "1". Float/int types are stringified by their real value.
    switch (f.type()) {
        case core::FieldType::Bool:
            if (auto b = f.as_bool()) return *b ? "1" : "0";
            break;
        case core::FieldType::Float32:
        case core::FieldType::Float64:
            if (auto d = f.as_float64()) {
                std::ostringstream os;
                os.precision(12);
                os << *d;
                return os.str();
            }
            break;
        default: break;
    }
    if (auto i = f.as_int64()) return std::to_string(*i);
    if (auto u = f.as_uint64()) return std::to_string(*u);
    if (auto d = f.as_float64()) {
        std::ostringstream os;
        os.precision(12);
        os << *d;
        return os.str();
    }
    if (auto b = f.as_bool()) return *b ? "1" : "0";
    return "";
}

namespace {

auto csv_escape(const std::string& s) -> std::string {
    bool needs = s.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

auto export_csv(const core::Block& block, const std::string& path) -> bool {
    std::ofstream out{path, std::ios::trunc};
    if (!out) return false;
    auto names = block.column_names();
    for (size_t c = 0; c < names.size(); ++c) {
        if (c) out << ",";
        out << csv_escape(names[c]);
    }
    out << "\n";
    const auto rows = block.row_count();
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < names.size(); ++c) {
            if (c) out << ",";
            out << csv_escape(field_to_string(block.get_row_value(c, r)));
        }
        out << "\n";
    }
    return true;
}

auto python_exe() -> std::string {
    if (const char* env = std::getenv("MNEMO_PYTHON"); env && *env) return env;
    return "python";
}

auto runtime_script() -> std::string {
    if (const char* env = std::getenv("MNEMO_ML_RUNTIME"); env && *env) return env;
    // Common locations relative to the server's working directory.
    for (const char* cand : {"ml_runtime/run.py", "../ml_runtime/run.py",
                             "../../ml_runtime/run.py"}) {
        std::error_code ec;
        if (fs::exists(cand, ec)) return cand;
    }
    return "ml_runtime/run.py";
}

auto quote(const std::string& s) -> std::string {
    return "\"" + s + "\"";
}

// Run a shell command, redirecting stdout+stderr to log_path. Returns exit code.
auto run_process(const std::string& cmd, const std::string& log_path) -> int {
    std::string full = cmd + " > " + quote(log_path) + " 2>&1";
#ifdef _WIN32
    // cmd.exe strips the outermost pair of quotes from the whole command line.
    full = "\"" + full + "\"";
#endif
    return std::system(full.c_str());
}

auto map_to_json(const std::map<std::string, std::string>& m) -> Value {
    Object o;
    for (const auto& [k, v] : m) o.emplace_back(k, Value{v});
    return Value{o};
}

auto features_to_json(const std::vector<std::string>& v) -> Value {
    Array a;
    for (const auto& s : v) a.emplace_back(Value{s});
    return Value{a};
}

} // namespace

auto ModelRuntime::run_dir(const std::string& run_id) -> std::string {
    auto dir = fs::path{models_dir()} / "runs" / run_id;
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

auto ModelRuntime::run(const RuntimeRequest& req) -> RuntimeResult {
    RuntimeResult result;

    std::string dir;
    try {
        dir = run_dir(req.run_id);
    } catch (const std::exception& e) {
        result.error = std::string{"cannot create run dir: "} + e.what();
        return result;
    }

    const auto spec_path = (fs::path{dir} / "spec.json").string();
    const auto result_path = (fs::path{dir} / "result.json").string();
    const auto log_path = (fs::path{dir} / "runtime.log").string();
    const auto artifact_path = (fs::path{dir} / "model.joblib").string();

    std::string data_path;
    if (req.data && req.data->row_count() > 0) {
        data_path = (fs::path{dir} / "data.csv").string();
        if (!export_csv(*req.data, data_path)) {
            result.error = "failed to export training/eval data to CSV";
            return result;
        }
    }

    // ── Build spec.json ──
    Object spec;
    spec.emplace_back("action", Value{req.action});
    spec.emplace_back("run_id", Value{req.run_id});
    spec.emplace_back("model", Value{req.model});
    spec.emplace_back("model_type", Value{req.model_type});
    spec.emplace_back("framework", Value{req.framework});
    spec.emplace_back("algorithm", Value{req.algorithm});
    spec.emplace_back("entrypoint", Value{req.entrypoint});
    spec.emplace_back("objective", Value{req.objective});
    spec.emplace_back("entity_key", Value{req.entity_key});
    spec.emplace_back("features", features_to_json(req.features));
    spec.emplace_back("target", Value{req.target});
    spec.emplace_back("hyperparameters", map_to_json(req.hyperparameters));
    spec.emplace_back("strategy", Value{req.strategy});
    spec.emplace_back("trials", Value{static_cast<int64_t>(req.trials)});
    spec.emplace_back("search_space", map_to_json(req.search_space));
    spec.emplace_back("artifact_in", Value{req.artifact_in});
    spec.emplace_back("artifact_out", Value{artifact_path});
    spec.emplace_back("predict_mode", Value{req.predict_mode});
    spec.emplace_back("predict_features", map_to_json(req.predict_features));
    spec.emplace_back("prompt", Value{req.prompt});
    spec.emplace_back("data_csv", Value{data_path});
    spec.emplace_back("result_path", Value{result_path});

    {
        std::ofstream sf{spec_path, std::ios::trunc};
        if (!sf) {
            result.error = "cannot write spec.json";
            return result;
        }
        sf << Value{spec}.dump();
    }

    // Remove any stale result so we don't read a previous run's output.
    std::error_code ec;
    fs::remove(result_path, ec);

    const std::string cmd =
        quote(python_exe()) + " " + quote(runtime_script()) + " --spec " + quote(spec_path);
    const int code = run_process(cmd, log_path);

    // ── Parse result.json ──
    if (!fs::exists(result_path, ec)) {
        std::string log;
        if (std::ifstream lf{log_path}; lf) {
            std::stringstream ss; ss << lf.rdbuf(); log = ss.str();
        }
        result.error = "ML runtime produced no result (exit " + std::to_string(code) +
                       "). Log:\n" + log;
        return result;
    }

    std::ifstream rf{result_path};
    std::stringstream ss; ss << rf.rdbuf();
    Value root;
    try {
        root = Value::parse(ss.str());
    } catch (const std::exception& e) {
        result.error = std::string{"failed to parse result.json: "} + e.what();
        return result;
    }

    result.raw = root;
    result.ok = root["ok"].as_bool(false);
    if (!result.ok) {
        result.error = root["error"].as_string();
        if (result.error.empty()) result.error = "ML runtime reported failure";
        return result;
    }

    for (const auto& [k, v] : root["metrics"].as_object())
        result.metrics[k] = v.as_number();

    result.artifact_location = root["artifact_location"].as_string();
    if (result.artifact_location.empty()) result.artifact_location = artifact_path;

    for (const auto& [k, v] : root["best_params"].as_object())
        result.best_params[k] = v.type() == json::Type::String ? v.as_string()
                                                               : std::to_string(v.as_number());

    for (const auto& t : root["trials"].as_array()) {
        TrialRecord tr;
        for (const auto& [k, v] : t["params"].as_object())
            tr.params[k] = v.type() == json::Type::String ? v.as_string()
                                                          : std::to_string(v.as_number());
        for (const auto& [k, v] : t["metrics"].as_object())
            tr.metrics[k] = v.as_number();
        tr.objective_value = t["objective_value"].as_number();
        result.trials.push_back(std::move(tr));
    }

    for (const auto& p : root["predictions"].as_array()) {
        PredictionOut po;
        po.entity_key = p["entity_key"].type() == json::Type::String
                            ? p["entity_key"].as_string()
                            : std::to_string(p["entity_key"].as_int());
        po.prediction = p["prediction"].type() == json::Type::String
                            ? p["prediction"].as_string()
                            : std::to_string(p["prediction"].as_number());
        po.confidence = p["confidence"].as_number();
        result.predictions.push_back(std::move(po));
    }

    result.generated_text = root["generated_text"].as_string();
    return result;
}

} // namespace mnemo::models
