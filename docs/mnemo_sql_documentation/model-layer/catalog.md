# Model Catalog (CREATE / SHOW / DESCRIBE / DROP)

Reference for all model-layer catalog entities: properties, SQL syntax, and introspection commands.

## FEATURE_SET

Explicitly declares model inputs. Mnemo **never infers** features or targets.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Feature set identifier |
| `source_table` | string | Table in `FROM` clause |
| `entity_key` | string | Primary entity column |
| `features` | list | Input feature column names |
| `target` | string | Label column (empty for unsupervised) |
| `version` | uint32 | Schema version |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

### CREATE

```sql
CREATE FEATURE_SET churn_features
    FROM ml_customers
    ENTITY_KEY(customer_id)
    FEATURES(tenure, monthly_charges, support_calls)
    TARGET churn;

-- Unsupervised (no TARGET)
CREATE FEATURE_SET customer_embeddings
    FROM customers
    ENTITY_KEY(customer_id)
    FEATURES(tenure, monthly_charges, region);
```

### SHOW / DESCRIBE / DROP

```sql
SHOW FEATURE_SETS;
DESCRIBE FEATURE_SET churn_features;
DROP FEATURE_SET churn_features;
DROP FEATURE_SET IF EXISTS churn_features;
```

---

## DATASET

Raw training data reference for deep learning, LLM, or RL workloads.

### Properties

| Property | Description |
|----------|-------------|
| `name` | Dataset identifier |
| `source` | Table name or filesystem path |
| `version` | Dataset version |

### CREATE

```sql
CREATE DATASET training_slice
    FROM ml_customers;

CREATE DATASET llm_corpus
    FROM '/data/corpus/train.jsonl';
```

```sql
SHOW DATASETS;
DESCRIBE DATASET training_slice;
DROP DATASET IF EXISTS training_slice;
```

---

## MODEL

Logical business intelligence asset.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Model name |
| `type` | enum | See supported types below |
| `description` | string | Human-readable description |
| `owner` | string | Owner |
| `status` | string | e.g. `REGISTERED` |
| `latest_version` | uint32 | 0 if never trained |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

### Supported types

`CLASSIFICATION`, `REGRESSION`, `FORECASTING`, `RECOMMENDATION`, `CLUSTERING`, `EMBEDDING`, `LLM`, `RL`, `CUSTOM`

### CREATE

```sql
CREATE MODEL churn_model TYPE CLASSIFICATION;
CREATE MODEL revenue_model TYPE REGRESSION;
CREATE MODEL product_rec TYPE RECOMMENDATION;
CREATE MODEL mnemo_llm TYPE LLM;

CREATE MODEL IF NOT EXISTS churn_model TYPE CLASSIFICATION
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier;
```

```sql
SHOW MODELS;
DESCRIBE MODEL churn_model;
DROP MODEL churn_model;
DROP MODEL IF EXISTS churn_model;
```

---

## MODEL_TEMPLATE

Reusable training preset.

### Properties

| Property | Description |
|----------|-------------|
| `name` | Template name |
| `framework` | `SKLEARN`, `XGBOOST`, … |
| `algorithm` | Algorithm class name |
| `hyperparams` | Default hyperparameter map |

### CREATE

```sql
CREATE MODEL_TEMPLATE rf_template
    TYPE CLASSIFICATION
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    HYPERPARAMS(n_estimators=100, max_depth=8);

SHOW MODEL_TEMPLATES;
DESCRIBE MODEL_TEMPLATE rf_template;
DROP MODEL_TEMPLATE IF EXISTS rf_template;
```

---

## TRAINING_JOB

Binds a model to a feature set with framework, algorithm, and hyperparameters.

### Properties

| Property | Description |
|----------|-------------|
| `name` | Job identifier |
| `model` | Target model name |
| `feature_set` | Feature set name |
| `dataset` | Optional dataset override |
| `framework` | `SKLEARN`, `XGBOOST`, … |
| `algorithm` | e.g. `RandomForestClassifier` |
| `entrypoint` | Custom script path |
| `objective` | `ACCURACY`, `F1`, `ROC_AUC`, … |
| `hyperparams` | Key-value parameter map |

See [training-and-jobs.md](training-and-jobs.md) for `RUN TRAINING_JOB`.

---

## TUNING_JOB

Hyperparameter search configuration.

### Properties

| Property | Description |
|----------|-------------|
| `name` | Tuning job identifier |
| `model` | Target model (optional if `TRAINING_JOB` set) |
| `training_job` | Base training job to extend |
| `strategy` | `GRID`, `RANDOM`, `OPTUNA`, `BAYESIAN`, … |
| `trials` | Number of search iterations |
| `objective` | Metric to optimize |
| `search_space` | Parameter range specs |

See [training-and-jobs.md](training-and-jobs.md) for `RUN TUNING_JOB`.

---

## MODEL_VERSION (created by RUN)

Immutable trained artifact. Not created directly — registered when `RUN TRAINING_JOB` or `RUN TUNING_JOB` succeeds.

### Properties

| Property | Description |
|----------|-------------|
| `model` | Parent model |
| `version` | `1`, `2`, … |
| `run_id` | Training run ID |
| `artifact_location` | Serialized model file path |
| `metrics` | Evaluation metrics map |

```sql
SHOW MODEL VERSIONS churn_model;
DESCRIBE MODEL VERSION churn_model:v1;
```

---

## MODEL_ENDPOINT (created by DEPLOY)

Named serving endpoint with monitoring counters.

### Properties

| Property | Description |
|----------|-------------|
| `name` | Endpoint alias |
| `model` | Parent model |
| `version` | Deployed version |
| `status` | `ACTIVE`, `STARTING`, `FAILED`, … |
| `prediction_count` | Total predictions |
| `failure_count` | Failed predictions |
| `total_latency_ms` | Cumulative latency |

```sql
SHOW MODEL ENDPOINTS churn_model;
SHOW MODEL METRICS churn_model;
SHOW MODEL DRIFT churn_model;
```

---

## Shared clause grammar

```ebnf
model_clauses ::= clause*
clause        ::= TYPE identifier
                | FRAMEWORK identifier
                | ALGORITHM identifier
                | ENTRYPOINT string
                | OBJECTIVE identifier
                | STRATEGY identifier
                | TRIALS integer
                | MODEL identifier
                | FEATURE_SET identifier
                | DATASET identifier
                | TRAINING_JOB identifier
                | FROM identifier
                | ENTITY_KEY '(' identifier ')'
                | FEATURES '(' identifier_list ')'
                | TARGET identifier
                | HYPERPARAMS '(' kv_list ')'
                | SEARCH_SPACE '(' kv_list ')'

kv_list       ::= key ( '=' | ':' ) value ( ',' key ( '=' | ':' ) value )*
```

## Hyperparameters and search space

```sql
-- Fixed hyperparameters
HYPERPARAMS(n_estimators=60, max_depth=6, random_state=42)

-- Tuning ranges
SEARCH_SPACE(n_estimators='20..120', max_depth='2..12')
SEARCH_SPACE(max_depth=3:10, n_estimators=50:200)
SEARCH_SPACE(C='0.01,0.1,1.0')
```

## Full SHOW catalog

```sql
SHOW MODELS;
SHOW MODEL VERSIONS churn_model;
SHOW FEATURE_SETS;
SHOW DATASETS;
SHOW TRAINING_JOBS;
SHOW TUNING_JOBS;
SHOW MODEL_TEMPLATES;
SHOW MODEL ENDPOINTS churn_model;
SHOW MODEL METRICS churn_model;
SHOW MODEL DRIFT churn_model;
```

## Full DESCRIBE catalog

```sql
DESCRIBE MODEL churn_model;
DESCRIBE MODEL VERSION churn_model:v1;
DESCRIBE FEATURE_SET churn_features;
DESCRIBE DATASET training_slice;
DESCRIBE TRAINING_JOB churn_training;
DESCRIBE TUNING_JOB tune_rf;
DESCRIBE MODEL_TEMPLATE rf_template;
```

## Full DROP catalog

```sql
DROP MODEL churn_model;
DROP FEATURE_SET churn_features;
DROP DATASET training_slice;
DROP TRAINING_JOB churn_training;
DROP TUNING_JOB tune_rf;
DROP MODEL_TEMPLATE rf_template;
```

Use `IF EXISTS` on all drops.

## Related

- [workflows.md](workflows.md) — step-by-step lifecycle
- [inference.md](inference.md) — prediction verbs
- `docs/PRDS/MODEL_LAYER_.md`
