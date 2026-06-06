# Training and Tuning Jobs

## CREATE TRAINING_JOB

```sql
CREATE TRAINING_JOB churn_training
    MODEL churn_model
    FEATURE_SET churn_features
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    OBJECTIVE ACCURACY
    HYPERPARAMS(n_estimators=60, max_depth=6);
```

Required references:

- `MODEL name` — target model catalog entry
- `FEATURE_SET name` — feature definition

Training configuration:

| Clause | Example |
|--------|---------|
| `FRAMEWORK` | `SKLEARN` |
| `ALGORITHM` | `RandomForestClassifier`, `LogisticRegression` |
| `OBJECTIVE` | `ACCURACY`, `F1`, metrics per PRD |
| `HYPERPARAMS(...)` | Algorithm parameters |
| `ENTRYPOINT` | Custom script path (when supported) |

## RUN TRAINING_JOB

```sql
RUN TRAINING_JOB churn_training;
```

On success:

- Registers a new model version (e.g. `v1`)
- Persists artifact path
- Returns metrics matching `OBJECTIVE`

Requires Python ML runtime dependencies (`scikit-learn`, `pandas`, `numpy`, `joblib`).

## CREATE TUNING_JOB

Hyperparameter search over a defined space:

```sql
CREATE TUNING_JOB tune_rf
    MODEL churn_model
    FEATURE_SET churn_features
    FRAMEWORK SKLEARN
    ALGORITHM RandomForestClassifier
    OBJECTIVE ACCURACY
    STRATEGY BAYESIAN
    TRIALS 20
    SEARCH_SPACE(max_depth=3:10, n_estimators=50:200);
```

## RUN TUNING_JOB

```sql
RUN TUNING_JOB tune_rf;
```

## SHOW / DESCRIBE / DROP

```sql
SHOW TRAINING_JOBS;
DESCRIBE TRAINING_JOB churn_training;
DROP TRAINING_JOB churn_training;

SHOW TUNING_JOBS;
DESCRIBE TUNING_JOB tune_rf;
DROP TUNING_JOB tune_rf;
```

## Error handling

```sql
RUN TRAINING_JOB definitely_missing_job;   -- fails: unknown job
```

Duplicate `CREATE MODEL` without `IF NOT EXISTS` fails when the name exists.

See `scripts/test_tuning_api.py` and `scripts/test_models_api.py`.
