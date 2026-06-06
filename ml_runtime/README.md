# Mnemosyne ML Runtime

This is the Python compute backend for the Mnemosyne **MODEL layer**. The C++
server orchestrates the ML lifecycle (catalog, versioning, registry,
deployment) and delegates the actual training, tuning, evaluation, and
inference to this runtime — keeping real ML logic inside the frameworks
(scikit-learn, XGBoost, Optuna, …) per MODEL_LAYER PRD Principle 4.

## How it works

For each `RUN TRAINING_JOB`, `RUN TUNING_JOB`, `EVALUATE`, `PREDICT`, or
`GENERATE`, the server:

1. Resolves the FEATURE_SET against real tables and exports the rows to
   `<run_dir>/data.csv`.
2. Writes `<run_dir>/spec.json` describing the action.
3. Spawns `python ml_runtime/run.py --spec <spec.json>`.
4. Reads `<run_dir>/result.json` and ingests metrics / artifacts / versions
   back into the model catalog.

Run directories live under `MNEMO_MODELS_DIR` (default `./mnemo_models/runs/<run_id>/`).

### spec.json (server → runtime)

```json
{
  "action": "train|tune|evaluate|predict|generate|explain",
  "run_id": "...",
  "model": "customer_churn",
  "model_type": "CLASSIFICATION",
  "framework": "SKLEARN",
  "algorithm": "RandomForestClassifier",
  "entity_key": "customer_id",
  "features": ["purchase_frequency", "avg_order_value"],
  "target": "churn",
  "hyperparameters": {"n_estimators": "100"},
  "strategy": "OPTUNA", "trials": 50, "search_space": {"max_depth": "2..16"},
  "artifact_in": "<path to load>", "artifact_out": "<path to save>",
  "predict_mode": "entity|features|batch",
  "predict_features": {"customer_id": "1234"},
  "data_csv": "<run_dir>/data.csv",
  "result_path": "<run_dir>/result.json"
}
```

### result.json (runtime → server)

```json
{
  "ok": true,
  "metrics": {"ROC_AUC": 0.91, "F1": 0.88},
  "artifact_location": "<run_dir>/model.joblib",
  "best_params": {"max_depth": "8"},
  "trials": [{"params": {...}, "metrics": {...}, "objective_value": 0.9}],
  "predictions": [{"entity_key": "1234", "prediction": "1", "confidence": 0.92}],
  "generated_text": "..."
}
```

If a required library is missing, the runtime returns `{"ok": false, "error": ...}`
with install guidance rather than crashing.

## Install

```bash
pip install -r ml_runtime/requirements.txt
```

## Environment variables

| Variable            | Default                | Purpose                                  |
| ------------------- | ---------------------- | ---------------------------------------- |
| `MNEMO_PYTHON`      | `python`               | Python executable the server spawns      |
| `MNEMO_ML_RUNTIME`  | `ml_runtime/run.py`    | Path to this dispatcher                  |
| `MNEMO_MODELS_DIR`  | `mnemo_models`         | Catalog + run-artifact directory         |
| `MNEMO_LLM_MODEL`   | `distilgpt2`           | HF model id for `GENERATE`               |

## Standalone use

You can drive the runtime directly without the server:

```bash
python ml_runtime/run.py --spec /path/to/spec.json
```
