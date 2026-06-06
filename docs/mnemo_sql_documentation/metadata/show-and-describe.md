# SHOW and DESCRIBE

## SHOW — databases and tables

```sql
SHOW;              -- databases (default)
SHOW DATABASES;
SHOW TABLES;
```

## SHOW — views

```sql
SHOW VIEWS;
SHOW MATERIALIZED VIEWS;
```

## SHOW — storage

```sql
SHOW STORAGE_UNITS;
SHOW STORAGE UNITS;    -- equivalent
SHOW STORAGE_USAGE;
SHOW STORAGE USAGE;
```

## SHOW — cluster topology

```sql
SHOW NODES;
SHOW NODE METRICS [ node_name ];
SHOW NODE CAPABILITIES [ node_name ];
SHOW NODE PARTITIONS [ node_name ];
SHOW NODE REPLICAS [ node_name ];
SHOW CLUSTERS;
SHOW REPLICA_GROUPS;
SHOW REPLICATION STATUS;
SHOW SHARD GROUPS;
SHOW SHARDS;
SHOW SHARD STATUS;
```

## SHOW — connectors

```sql
SHOW CONNECTORS;
SHOW CONNECTOR CAPABILITIES [ connector_name ];
SHOW CONNECTOR STATUS [ connector_name ];
```

## SHOW — pipelines

```sql
SHOW PIPELINES;
SHOW STAGES [ FROM PIPELINE pipeline_name ];
SHOW TASKS [ FROM PIPELINE pipeline_name ];
SHOW TRIGGERS [ FROM PIPELINE pipeline_name ];
SHOW PIPELINE RUNS [ FOR PIPELINE pipeline_name ];
SHOW PIPELINE METRICS [ FOR PIPELINE pipeline_name ];
```

## SHOW — streams

```sql
SHOW STREAMS;
SHOW TOPICS;
SHOW CONSUMER_GROUPS;
SHOW STREAM METRICS [ FOR STREAM stream_name ];
```

## SHOW — model layer

```sql
SHOW MODELS;
SHOW MODEL VERSIONS [ model_name ];
SHOW MODEL ENDPOINTS [ model_name ];
SHOW MODEL METRICS [ model_name ];
SHOW MODEL DRIFT [ model_name ];
SHOW FEATURE_SETS;
SHOW DATASETS;
SHOW TRAINING_JOBS;
SHOW TUNING_JOBS;
SHOW MODEL_TEMPLATES;
```

## DESCRIBE

General form: `DESCRIBE [ object_kind ] name`

```sql
DESCRIBE customers;                    -- table (default)
DESCRIBE TABLE customers;
DESCRIBE VIEW active_customers;
DESCRIBE MATERIALIZED VIEW age_counts;
DESCRIBE STORAGE_UNIT su_local;
DESCRIBE NODE worker_01;
DESCRIBE CLUSTER main;
DESCRIBE REPLICA_GROUP standard_ha;
DESCRIBE SHARD_GROUP customer_distribution;
DESCRIBE CONNECTOR api_rest;
DESCRIBE PIPELINE etl_sync;
DESCRIBE STREAM events;
DESCRIBE TOPIC user_activity;
DESCRIBE CONSUMER_GROUP analytics;
DESCRIBE MODEL churn_model;
DESCRIBE MODEL VERSION churn_model:v1;
DESCRIBE FEATURE_SET churn_features;
DESCRIBE DATASET training_data;
DESCRIBE TRAINING_JOB churn_training;
DESCRIBE TUNING_JOB hyperparam_search;
DESCRIBE MODEL_TEMPLATE rf_template;
```

Model version syntax accepts `model_name:vN` or colon-separated numeric version after `DESCRIBE MODEL VERSION`.

## Result shape

`SHOW` and `DESCRIBE` return tabular JSON over HTTP (columns + rows), suitable for dashboards and CLI formatting.
