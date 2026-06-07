# Inference and Model Operations

Deploy trained versions, score predictions, evaluate metrics, compare models, and generate LLM text.

## DEPLOY MODEL

Expose a model version as a named endpoint for serving and monitoring.

```sql
DEPLOY MODEL churn_model:1 AS churn_endpoint;
DEPLOY MODEL churn_model:v1 AS churn_endpoint;
DEPLOY MODEL churn_model AS churn_latest;   -- latest version when supported
```

### Endpoint properties (after deploy)

| Property | Description |
|----------|-------------|
| `name` | Endpoint alias (`churn_endpoint`) |
| `model` | Parent model |
| `version` | Deployed version number |
| `status` | `ACTIVE`, `STARTING`, `FAILED`, `STOPPED` |
| `artifact_location` | Model file path |
| `prediction_count` | Total predictions served |
| `failure_count` | Failed requests |
| `total_latency_ms` | Cumulative latency |

```sql
SHOW MODEL ENDPOINTS churn_model;
SHOW MODEL METRICS churn_model;
SHOW MODEL DRIFT churn_model;
```

---

## PREDICT MODEL

Three scoring modes:

### WITH — direct feature vector

Score without entity lookup. Provide all feature columns:

```sql
PREDICT MODEL churn_model:1 WITH (
    tenure=2,
    monthly_charges=95.0,
    support_calls=5
);
```

**Response fields:** `prediction`, `confidence` (and related metadata).

Use for real-time API scoring when features arrive in the request payload.

### FOR — entity lookup

Resolve features from the training `FEATURE_SET` by `ENTITY_KEY`:

```sql
PREDICT MODEL churn_model:1 FOR (customer_id=2);
```

**Response fields:** `entity_key`, `prediction`, `confidence`.

The runtime loads the feature set bound to the trained version's training job and looks up the entity row in `source_table`.

### FROM — batch prediction

Score all rows from a table and materialize results:

```sql
PREDICT MODEL churn_model:1 FROM ml_customers;
```

Creates a **prediction table** named `{model_name}_predictions` (e.g. `churn_model_predictions`).

```sql
SELECT * FROM churn_model_predictions LIMIT 10;
SELECT COUNT(*) AS n FROM churn_model_predictions;

-- Join back to source for analysis
SELECT
    c.customer_id,
    c.churn AS actual,
    p.prediction AS predicted,
    p.confidence
FROM ml_customers c
JOIN churn_model_predictions p
    ON cast(c.customer_id AS String) = p.entity_key;
```

### Prediction record schema

| Field | Description |
|-------|-------------|
| `entity_key` | Entity identifier (batch mode) |
| `prediction` | Class label or regression value |
| `confidence` | Probability or score |
| `model_version` | Version string |
| `timestamp` | Prediction time |

---

## EVALUATE MODEL

Compute metrics for a trained version:

```sql
EVALUATE MODEL churn_model:1;
EVALUATE MODEL churn_model:v1;
```

Returns metrics aligned with training `OBJECTIVE` (e.g. `ACCURACY`, `F1`). Run before `DEPLOY` to gate production releases.

```sql
-- Gate deploy on accuracy threshold (application logic)
-- 1. EVALUATE MODEL churn_model:1
-- 2. If ACCURACY >= 0.80 then DEPLOY ...
```

---

## COMPARE MODELS

Side-by-side metric comparison:

```sql
COMPARE MODELS churn_model:1, churn_model_b:1;
COMPARE MODEL churn_model:1, churn_model_b:1;
```

Use after training multiple algorithms on the same `FEATURE_SET`:

```sql
CREATE MODEL churn_rf TYPE CLASSIFICATION;
CREATE MODEL churn_lr TYPE CLASSIFICATION;

CREATE TRAINING_JOB train_rf MODEL churn_rf FEATURE_SET churn_features
    FRAMEWORK SKLEARN ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY;
CREATE TRAINING_JOB train_lr MODEL churn_lr FEATURE_SET churn_features
    FRAMEWORK SKLEARN ALGORITHM LogisticRegression OBJECTIVE ACCURACY;

RUN TRAINING_JOB train_rf;
RUN TRAINING_JOB train_lr;

COMPARE MODELS churn_rf:1, churn_lr:1;
```

---

## GENERATE

LLM text generation (when model type is `LLM` and runtime dependencies exist):

```sql
GENERATE USING MODEL my_llm PROMPT 'Summarize this customer segment';
GENERATE MODEL my_llm PROMPT 'Hello';
```

Requires `transformers` and `torch` (see `ml_runtime/README.md`). Set `MNEMO_LLM_MODEL` environment variable (default `distilgpt2`).

```bash
pip install transformers torch
export MNEMO_LLM_MODEL=distilgpt2
```

---

## Application integration

### HTTP

```bash
curl -G "http://127.0.0.1:1143/query" \
  --data-urlencode "query=PREDICT MODEL churn_model:1 FOR (customer_id=42)" \
  --data-urlencode "format=JSON"
```

### Python

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")

# Single scoring
r = client.query(
    "PREDICT MODEL churn_model:1 WITH "
    "(tenure=2, monthly_charges=95.0, support_calls=5)"
)
prediction = r.json()["data"][0]

# Batch scoring
client.query("PREDICT MODEL churn_model:1 FROM ml_customers")
r = client.query("SELECT COUNT(*) FROM churn_model_predictions")
```

### Monitoring loop

```python
import time
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
while True:
    metrics = client.query("SHOW MODEL METRICS churn_model")
    print(metrics.body)
    time.sleep(60)
```

---

## Complete inference workflow

```sql
-- Prerequisites: RUN TRAINING_JOB completed → v1 exists

EVALUATE MODEL churn_model:1;

DEPLOY MODEL churn_model:1 AS churn_production;

-- Online scoring
PREDICT MODEL churn_model:1 FOR (customer_id=1001);

-- Batch scoring for reporting
PREDICT MODEL churn_model:1 FROM ml_customers;

-- Monitor
SHOW MODEL ENDPOINTS churn_model;
SHOW MODEL METRICS churn_model;
SHOW MODEL DRIFT churn_model;
```

---

## Visualization queries

See [visualizations.md](visualizations.md) for chart-ready SQL on prediction tables:

```sql
-- Confidence distribution
SELECT floor(confidence * 10) / 10 AS bucket, count(*) AS n
FROM churn_model_predictions
GROUP BY bucket ORDER BY bucket;

-- Actual vs predicted
SELECT c.churn AS actual, p.prediction AS predicted, count(*) AS n
FROM ml_customers c
JOIN churn_model_predictions p ON cast(c.customer_id AS String) = p.entity_key
GROUP BY actual, predicted;
```

---

## Related

- [workflows.md](workflows.md) — training through deployment
- [catalog.md](catalog.md) — entity properties
- `ml_runtime/README.md`
- `scripts/test_models_api.py`
