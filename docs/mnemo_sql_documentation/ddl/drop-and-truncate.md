# DROP and TRUNCATE

## DROP TABLE

```sql
DROP TABLE orders;
DROP TABLE IF EXISTS orders;
```

## DROP VIEW / MATERIALIZED VIEW

```sql
DROP VIEW active_customers;
DROP MATERIALIZED VIEW age_counts;
DROP VIEW IF EXISTS v;
```

## DETACH

`DETACH` is parsed as a drop variant for some storage backends (detach without deleting files):

```sql
DETACH TABLE large_table;
```

Behavior is engine-specific.

## TRUNCATE TABLE

Delete all rows; keep schema:

```sql
TRUNCATE TABLE orders;
```

## DROP entity objects

Mnemo extends `DROP` to first-class entities:

```sql
DROP STORAGE_UNIT su_local;
DROP CONNECTOR api_rest;
DROP NODE worker_01;          -- prefer REMOVE NODE for registered nodes
DROP PIPELINE etl_pipeline;
DROP STAGE ingest FROM PIPELINE etl_pipeline;
DROP TASK task_a FROM STAGE s FROM PIPELINE p IN PIPELINE ...;
DROP STREAM events;
DROP TOPIC user_activity;
DROP CONSUMER_GROUP analytics;
DROP MODEL churn_model;
DROP FEATURE_SET churn_features;
DROP TRAINING_JOB churn_training;
DROP TUNING_JOB hyperparam_search;
DROP MODEL_TEMPLATE rf_template;
DROP CLUSTER production;
DROP DATA_QUALITY_RULE valid_email;
```

Use `IF EXISTS` to avoid errors when the object is missing.

`DROP CLUSTER` detaches the cluster's member nodes and leaves them registered; see
[entities/nodes-and-clusters.md](../entities/nodes-and-clusters.md#alter-cluster).

## DROP DATABASE

```sql
DROP DATABASE analytics;
DROP DATABASE IF EXISTS analytics;
```

Removes the database **and deletes its tables' data from disk**. This cannot be undone. Storage
unit, shard group and replica group references held by its tables are released.

## Examples from tests

```sql
DROP TABLE IF EXISTS customers;
DROP MATERIALIZED VIEW IF EXISTS age_counts;
DROP CONNECTOR IF EXISTS api_rest;
DROP PIPELINE IF EXISTS api_revenue_sync;
```
