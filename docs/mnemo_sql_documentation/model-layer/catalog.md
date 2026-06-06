# Model Catalog (CREATE / SHOW / DESCRIBE / DROP)

## CREATE MODEL

```sql
CREATE MODEL churn_model TYPE CLASSIFICATION;
CREATE MODEL revenue_model TYPE REGRESSION;
CREATE MODEL IF NOT EXISTS churn_model TYPE CLASSIFICATION;
```

Model types (examples): `CLASSIFICATION`, `REGRESSION` (see PRD for full enum).

Optional clauses (shared with other model objects):

```sql
CREATE MODEL m TYPE CLASSIFICATION
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier;
```

## CREATE FEATURE_SET

```sql
CREATE FEATURE_SET churn_features
    FROM ml_customers
    ENTITY_KEY(customer_id)
    FEATURES(tenure, monthly_charges, support_calls)
    TARGET churn;
```

| Clause | Purpose |
|--------|---------|
| `FROM table` | Source table for features |
| `ENTITY_KEY(col)` | Primary entity identifier |
| `FEATURES(cols…)` | Input columns |
| `TARGET col` | Label column for supervised learning |

## CREATE DATASET

```sql
CREATE DATASET training_slice
    FROM ml_customers
    MODEL churn_model;
```

## CREATE TRAINING_JOB / TUNING_JOB

See [training-and-jobs.md](training-and-jobs.md).

## CREATE MODEL_TEMPLATE

```sql
CREATE MODEL_TEMPLATE rf_template
    TYPE CLASSIFICATION
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    HYPERPARAMS(n_estimators=100);
```

## SHOW catalog

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

## DESCRIBE

```sql
DESCRIBE MODEL churn_model;
DESCRIBE MODEL VERSION churn_model:v1;
DESCRIBE FEATURE_SET churn_features;
DESCRIBE DATASET training_slice;
DESCRIBE TRAINING_JOB churn_training;
DESCRIBE TUNING_JOB tune_rf;
DESCRIBE MODEL_TEMPLATE rf_template;
```

## DROP

```sql
DROP MODEL churn_model;
DROP FEATURE_SET churn_features;
DROP DATASET training_slice;
DROP TRAINING_JOB churn_training;
DROP TUNING_JOB tune_rf;
DROP MODEL_TEMPLATE rf_template;
```

Use `IF EXISTS` on all drops.

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
                | HYPERPARAMS '(' kv_list ')'
                | SEARCH_SPACE '(' kv_list ')'

kv_list       ::= key ( '=' | ':' ) value ( ',' key ( '=' | ':' ) value )*
```

## Hyperparameters

```sql
HYPERPARAMS(n_estimators=60, max_depth=6, random_state=42)
```

Keys and values are parsed into a string map for the runtime.
