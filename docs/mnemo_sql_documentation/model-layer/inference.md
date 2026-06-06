# Inference and Model Operations

## DEPLOY MODEL

Expose a model version as a named endpoint:

```sql
DEPLOY MODEL churn_model:1 AS churn_endpoint;
DEPLOY MODEL churn_model AS churn_endpoint;   -- latest version when supported
```

Optional keyword `MODEL` after `DEPLOY`:

```sql
DEPLOY MODEL churn_model:1 AS churn_endpoint;
```

## PREDICT MODEL

Three modes parsed by the SQL grammar:

### WITH — direct feature vector

```sql
PREDICT MODEL churn_model:1 WITH (
    tenure=2,
    monthly_charges=95.0,
    support_calls=5
);
```

Parenthesized `key=value` pairs (comma-separated).

### FOR — entity lookup

Resolve features from the feature set by entity key:

```sql
PREDICT MODEL churn_model:1 FOR (customer_id=2);
```

Uses `ENTITY_KEY` from the bound feature set.

### FROM — batch prediction

Score all rows from a table; materializes a prediction table:

```sql
PREDICT MODEL churn_model:1 FROM ml_customers;
```

Default prediction table name pattern: `{model_name}_predictions` (verify in interpreter).

Query results:

```sql
SELECT COUNT(*) FROM churn_model_predictions;
```

## EVALUATE MODEL

Compute hold-out or training metrics for a version:

```sql
EVALUATE MODEL churn_model:1;
```

Returns metrics aligned with training `OBJECTIVE` (e.g. `ACCURACY`).

## COMPARE MODELS

Compare metrics across versions or models:

```sql
COMPARE MODELS churn_model:1, churn_model_b:1;
COMPARE MODEL churn_model:1, churn_model_b:1;
```

Comma-separated `model:version` list.

## GENERATE

LLM-style generation hook (when model type supports it):

```sql
GENERATE USING MODEL my_llm PROMPT 'Summarize this customer segment';
GENERATE MODEL my_llm PROMPT 'Hello';
```

## Monitoring

After deployment and prediction:

```sql
SHOW MODEL ENDPOINTS churn_model;
SHOW MODEL METRICS churn_model;
SHOW MODEL DRIFT churn_model;
```

## Endpoints in application code

Applications call `PREDICT` via SQL over HTTP the same as analytical queries. Deploy registers endpoint metadata for routing and metrics (`prediction_count`, latency, etc.).

See `ml_runtime/README.md` for runtime behavior.
