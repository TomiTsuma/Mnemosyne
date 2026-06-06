# MODEL Layer — changelog

**Date:** 2026-06-06

## Summary

Implemented the Mnemosyne **MODEL layer** end-to-end ([docs/PRDS/MODEL_LAYER_.md](../PRDS/MODEL_LAYER_.md)),
together with its `FEATURE_SET` (full) and `DATASET` (minimal) dependencies.

Models, versions, runs, training/tuning jobs, templates, and endpoints are
first-class catalog entities. The C++ server orchestrates the ML lifecycle and
delegates the actual compute — training, hyperparameter tuning, evaluation,
prediction, and generation — to a Python ML runtime (`ml_runtime/`) spawned as a
subprocess, keeping real ML logic inside scikit-learn / XGBoost / Optuna
(PRD Principle 4: framework independence).

The model registry, runs, versions, and artifacts persist to disk under
`MNEMO_MODELS_DIR` (default `./mnemo_models/`) for reproducibility — an
intentional deviation from the in-memory-only pattern of other layers.

## Changes by file

### New module: `src/FeatureSets/` (`mnemosyne_feature_sets`)

- `feature_set_catalog.h` — `FeatureSetEntry`, `DatasetEntry`, enums, name helpers
- `feature_set_manager.{h,cpp}` — singleton CRUD + versioning for feature sets / datasets
- `feature_set_resolver.{h,cpp}` — resolve a feature set against a real table into a `core::Block`
- `CMakeLists.txt`

### New module: `src/Models/` (`mnemosyne_models`)

- `model_catalog.h` — `Model/Version/Run/TrainingJob/TuningJob/Template/Endpoint/Prediction`
  entries + enums (`ModelType`, `Framework`, `RunStatus`, `EndpointStatus`,
  `TuningStrategy`, `Objective`) + helpers
- `model_manager.{h,cpp}` — registry, version auto-increment, run lifecycle,
  best-run selection, monitoring counters, JSON persistence (`catalog.json`)
- `model_runtime.{h,cpp}` — C++↔Python bridge: build run dir, export data to CSV,
  write `spec.json`, spawn the runtime, parse `result.json`, ingest results
- `json.h` — minimal self-contained JSON parser/serializer
- `CMakeLists.txt`

### Python ML runtime: `ml_runtime/`

- `run.py` (dispatcher), `algorithms.py`, `trainers.py`, `tuning.py`
  (Optuna with GRID/RANDOM fallback), `evaluation.py`, `prediction.py`,
  `explain.py`, `llm.py`, `requirements.txt`, `README.md`

### Parser / Analyzer / Planner

- Lexer (`lexer.{h,cpp}`): MODEL/FEATURE_SET keywords + `Colon` token for `model:vN`
- `ast.h`: query types, object kinds, `Create::Kind`, `Show::ShowType`,
  `ModelControl` struct, create/show/describe fields
- `parser.cpp`: CREATE branches (incl. `ENTITY_KEY/FEATURES/TARGET/FROM` and
  `HYPERPARAMS/SEARCH_SPACE`), SHOW/DESCRIBE/DROP branches (incl. `MODEL VERSION m:vN`),
  and `parse_deploy/predict/evaluate/compare/generate` plus `RUN TRAINING_JOB/TUNING_JOB`
- `analyzer.cpp` / `query_tree.h`: SHOW/DESCRIBE mappings + `model_name`/`version` fields;
  control verbs pass through to the direct dispatch path
- `planner.cpp` / `execution_plan.h`: SHOW `show_type` strings + DESCRIBE model version

### Interpreters

- `interpreter_create_query.{h,cpp}` — `do_create_model/feature_set/dataset/training_job/tuning_job/model_template`
- `interpreter_drop_query.cpp` — drop the new object kinds
- `interpreter_model_control.{h,cpp}` (new) — `RUN TRAINING_JOB/TUNING_JOB`, `DEPLOY`,
  `PREDICT` (FOR/WITH/FROM incl. `PREDICTION_TABLE` materialization), `EVALUATE`,
  `COMPARE`, `GENERATE`
- `block_interpreter.cpp` — SHOW/DESCRIBE for model/feature-set entities

### Server / Build

- `http_handler.cpp` — direct dispatch for `DEPLOY/PREDICT/EVALUATE/COMPARE/GENERATE`
  and `RUN` routing to training/tuning jobs
- `server.cpp` — `ModelManager::load()` at startup
- Root `CMakeLists.txt` — `add_subdirectory(src/FeatureSets)` + `add_subdirectory(src/Models)`
- `src/Interpreters/CMakeLists.txt`, `src/Server/CMakeLists.txt` — link the new libs

### Tests

- `scripts/test_ml_runtime.py` — 12 checks (standalone train/evaluate/predict/tune)
- `scripts/test_feature_sets_api.py` — 28 checks (FEATURE_SET / DATASET CRUD)
- `scripts/test_models_api.py` — 54 checks (full PRD §24 lifecycle + §25 expectations)
- `scripts/test_tuning_api.py` — 27 checks (RANDOM sweep + best-version registration)

## Bugs fixed during integration

- `model_runtime.cpp` `field_to_string`: integer columns were collapsing to `1`
  because `Field::as_bool()` treats any non-zero integer as `true`. Now only
  genuine `Bool` fields use the bool path; integers/floats stringify by value.
  (This had silently corrupted both training data and entity/batch prediction.)
- `ml_runtime/prediction.py`: replaced `pd.to_numeric(errors="ignore")` (removed
  in pandas 3.0) with a coerce-and-fallback per column.
- `interpreter_model_control.cpp`: `PREDICT ... FOR (entity)` now resolves the
  trained version's training feature set so the runtime can locate the entity row.

## SQL now supported

```sql
CREATE FEATURE_SET churn_features FROM customers
  ENTITY_KEY(customer_id) FEATURES(tenure, monthly_charges) TARGET churn;
CREATE DATASET churn_dataset FROM customers;

CREATE MODEL churn_model TYPE CLASSIFICATION;
CREATE TRAINING_JOB churn_training MODEL churn_model FEATURE_SET churn_features
  FRAMEWORK SKLEARN ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY
  HYPERPARAMS(n_estimators=100, max_depth=6);
RUN TRAINING_JOB churn_training;

CREATE TUNING_JOB churn_sweep TRAINING_JOB churn_training
  STRATEGY OPTUNA TRIALS 50 OBJECTIVE ACCURACY
  SEARCH_SPACE(n_estimators='20..300', max_depth='2..16');
RUN TUNING_JOB churn_sweep;

EVALUATE MODEL churn_model:1;
DEPLOY MODEL churn_model:1 AS churn_endpoint;
PREDICT MODEL churn_model:1 WITH (tenure=2, monthly_charges=95.0);
PREDICT MODEL churn_model:1 FOR (customer_id=42);
PREDICT MODEL churn_model:1 FROM customers;        -- materializes churn_model_predictions
COMPARE MODELS churn_model:1, churn_model_b:1;
GENERATE USING MODEL my_llm PROMPT 'summarize: ...';

SHOW MODELS | MODEL VERSIONS m | MODEL ENDPOINTS m | MODEL METRICS m | MODEL DRIFT m;
SHOW FEATURE_SETS | DATASETS | TRAINING_JOBS | TUNING_JOBS | MODEL_TEMPLATES;
DESCRIBE MODEL m | MODEL VERSION m:vN | FEATURE_SET fs | TRAINING_JOB tj | TUNING_JOB uj;

DROP MODEL|MODEL_TEMPLATE|TRAINING_JOB|TUNING_JOB|FEATURE_SET|DATASET name [IF EXISTS];
```

## Verification

```bash
cmake --build build --config Release --target mnemosyne_server
pip install -r ml_runtime/requirements.txt          # scikit-learn at minimum

python scripts/test_ml_runtime.py                    # standalone, no server
python scripts/test_feature_sets_api.py
python scripts/test_models_api.py
python scripts/test_tuning_api.py
```

## Out of scope / minimal

- Security/permissions (§23): metadata only, not enforced (no auth layer yet).
- Drift (§21): prediction volume / failure / latency counters per endpoint;
  not a full statistical drift engine.
- LLM/RLHF (`GENERATE`): scaffolded; runs if `transformers` is installed, otherwise
  returns a clear "LLM backend not installed" result.
