# Training and Tuning Jobs

Training jobs fit models; tuning jobs search hyperparameters. Both execute via the Python ML runtime on `RUN`.

## CREATE TRAINING_JOB

```sql
CREATE TRAINING_JOB churn_training
    MODEL churn_model
    FEATURE_SET churn_features
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    OBJECTIVE ACCURACY
    HYPERPARAMS(n_estimators=60, max_depth=6, random_state=42);
```

### Required clauses

| Clause | Description |
|--------|-------------|
| `MODEL name` | Target model catalog entry |
| `FEATURE_SET name` | Feature definition with `FROM`, `ENTITY_KEY`, `FEATURES`, `TARGET` |

### Optional clauses

| Clause | Example | Description |
|--------|---------|-------------|
| `FRAMEWORK` | `SKLEARN` | ML framework |
| `ALGORITHM` | `RandomForestClassifier` | Estimator class |
| `OBJECTIVE` | `ACCURACY`, `F1`, `ROC_AUC` | Metric to report |
| `HYPERPARAMS(...)` | `n_estimators=100` | Algorithm parameters |
| `DATASET` | `training_slice` | Override data source |
| `ENTRYPOINT` | `'/scripts/train.py'` | Custom training script |

### Frameworks

| Framework | Status |
|-----------|--------|
| `SKLEARN` | Fully supported |
| `XGBOOST` | Supported when installed |
| `LIGHTGBM`, `CATBOOST` | Supported when installed |
| `PYTORCH`, `TENSORFLOW` | Via `ENTRYPOINT` |
| `CUSTOM` | Via `ENTRYPOINT` |

---

## RUN TRAINING_JOB

```sql
RUN TRAINING_JOB churn_training;
```

### What happens

1. Server loads `TRAINING_JOB` and resolves `FEATURE_SET` → source table
2. Spawns Python ML runtime with framework, algorithm, hyperparams
3. Fits model on feature columns; evaluates against `OBJECTIVE`
4. Serializes artifact to disk (`artifact_location`)
5. Registers `MODEL_VERSION` (e.g. `v1`)
6. Returns JSON with `SUCCEEDED`, metrics, version id

### Example success response fields

| Field | Example |
|-------|---------|
| `status` | `SUCCEEDED` |
| `version` | `v1` |
| `ACCURACY` | `0.85` |
| `artifact` | `/path/to/model.joblib` |

```sql
SHOW MODEL VERSIONS churn_model;
DESCRIBE MODEL VERSION churn_model:v1;
```

### Prerequisites

```bash
pip install scikit-learn pandas numpy joblib
export MNEMO_PYTHON=/path/to/venv/bin/python  # optional
```

Without dependencies, `RUN TRAINING_JOB` fails with a clear missing-library error.

---

## CREATE TUNING_JOB

Hyperparameter search over a training configuration.

### Option A: Reference existing training job

```sql
CREATE TUNING_JOB tune_rf
    TRAINING_JOB churn_training
    STRATEGY RANDOM
    TRIALS 6
    OBJECTIVE ACCURACY
    SEARCH_SPACE(n_estimators='20..120', max_depth='2..12');
```

### Option B: Explicit model + feature set

```sql
CREATE TUNING_JOB tune_rf_explicit
    MODEL churn_model
    FEATURE_SET churn_features
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    STRATEGY BAYESIAN
    TRIALS 20
    OBJECTIVE ACCURACY
    SEARCH_SPACE(max_depth=3:10, n_estimators=50:200);
```

### TUNING_JOB properties

| Property | Description |
|----------|-------------|
| `name` | Job identifier |
| `training_job` | Base job (Option A) |
| `model` / `feature_set` | Direct binding (Option B) |
| `strategy` | Search algorithm |
| `trials` | Iteration count |
| `objective` | Metric to maximize |
| `search_space` | Parameter ranges |

### Strategies

| Strategy | Requires | Notes |
|----------|----------|-------|
| `RANDOM` | scikit-learn | Default fallback |
| `GRID` | scikit-learn | Exhaustive grid |
| `OPTUNA` | optuna | Bayesian optimization |
| `BAYESIAN` | optuna (falls back to RANDOM) | PRD alias |
| `HYPEROPT`, `EVOLUTIONARY` | varies | See runtime |

### Search space syntax

```sql
-- Range with ..
SEARCH_SPACE(n_estimators='20..120', max_depth='2..12')

-- Range with :
SEARCH_SPACE(max_depth=3:10, n_estimators=50:200)

-- Discrete list
SEARCH_SPACE(C='0.01,0.1,1.0', kernel='linear,rbf')
```

---

## RUN TUNING_JOB

```sql
RUN TUNING_JOB tune_rf;
```

### Response structure

- One row per trial: `trial`, `params`, `objective_value`
- Summary row: `best -> vN` with winning hyperparameters
- Best configuration registers a new `MODEL_VERSION`

```sql
-- After tuning
SHOW MODEL VERSIONS churn_model;
PREDICT MODEL churn_model:1 WITH (tenure=2, monthly_charges=95.0, support_calls=5);
```

---

## Feature engineering before training

Training reads directly from the `FEATURE_SET` source table. Prepare data with SQL first:

```sql
-- Derived features
CREATE TABLE ml_enriched ENGINE = Memory AS
SELECT
    customer_id,
    tenure,
    monthly_charges,
    support_calls,
    monthly_charges / greatest(tenure, 1) AS charge_velocity,
    churn
FROM raw_customers;

CREATE FEATURE_SET churn_features_v2
    FROM ml_enriched
    ENTITY_KEY(customer_id)
    FEATURES(tenure, monthly_charges, support_calls, charge_velocity)
    TARGET churn;

CREATE TRAINING_JOB churn_v2
    MODEL churn_model
    FEATURE_SET churn_features_v2
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    OBJECTIVE ACCURACY
    HYPERPARAMS(n_estimators=100, max_depth=8);

RUN TRAINING_JOB churn_v2;
```

---

## Python workflow

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")

# Training
client.query(
    "CREATE TRAINING_JOB churn_training MODEL churn_model "
    "FEATURE_SET churn_features FRAMEWORK SKLEARN "
    "ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY "
    "HYPERPARAMS(n_estimators=60, max_depth=6)"
)
r = client.query("RUN TRAINING_JOB churn_training")
assert r.ok() and "succeeded" in r.body.lower()

# Tuning
client.query(
    "CREATE TUNING_JOB tune_rf TRAINING_JOB churn_training "
    "STRATEGY RANDOM TRIALS 6 OBJECTIVE ACCURACY "
    "SEARCH_SPACE(n_estimators='20..120', max_depth='2..12')"
)
r = client.query("RUN TUNING_JOB tune_rf")
assert r.ok()
```

---

## SHOW / DESCRIBE / DROP

```sql
SHOW TRAINING_JOBS;
DESCRIBE TRAINING_JOB churn_training;
DROP TRAINING_JOB churn_training;
DROP TRAINING_JOB IF EXISTS churn_training;

SHOW TUNING_JOBS;
DESCRIBE TUNING_JOB tune_rf;
DROP TUNING_JOB tune_rf;
DROP TUNING_JOB IF EXISTS tune_rf;
```

`SHOW TRAINING_JOBS` returns columns: `name`, `model`, `feature_set`, `framework`, `algorithm`, `objective`.

---

## Error handling

| Scenario | Result |
|----------|--------|
| `RUN TRAINING_JOB missing` | Error: unknown job |
| `CREATE MODEL dup` without `IF NOT EXISTS` | Error: already exists |
| Missing scikit-learn | Error: library not importable |
| Invalid feature column | Runtime training failure |
| `RUN TUNING_JOB` with 0 trials | Error or no-op per validation |

---

## Pipeline integration

Schedule retraining:

```sql
CREATE PIPELINE weekly_retrain OWNER 'ml-ops';
CREATE STAGE train IN PIPELINE weekly_retrain ORDER 1;
CREATE TASK refresh_features IN STAGE train IN PIPELINE weekly_retrain
    TYPE SQL BODY 'REFRESH MATERIALIZED VIEW ml_feature_snapshot';
CREATE TASK run_training IN STAGE train IN PIPELINE weekly_retrain
    TYPE SQL BODY 'RUN TRAINING_JOB churn_training'
    DEPENDS ON refresh_features;
CREATE TRIGGER sunday ON PIPELINE weekly_retrain SCHEDULE '0 3 * * 0';

RUN PIPELINE weekly_retrain;
```

---

## Related

- [workflows.md](workflows.md) — complete lifecycle
- [inference.md](inference.md) — post-training evaluation and deploy
- [visualizations.md](visualizations.md) — tuning trial charts
- `scripts/test_models_api.py`, `scripts/test_tuning_api.py`
