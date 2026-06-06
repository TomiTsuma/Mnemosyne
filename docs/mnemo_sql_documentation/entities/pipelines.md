# Pipelines

Pipelines orchestrate staged tasks (SQL, built-in steps) with dependencies and triggers.

## Hierarchy

```
PIPELINE
  └── STAGE (ordered)
        └── TASK (typed, optional DEPENDS ON)
TRIGGER (on pipeline, schedule)
```

## CREATE PIPELINE

```sql
CREATE PIPELINE api_revenue_sync OWNER 'analytics';
CREATE PIPELINE IF NOT EXISTS etl OWNER 'data-team';
```

## CREATE STAGE

```sql
CREATE STAGE ingestion IN PIPELINE api_revenue_sync ORDER 1;
CREATE STAGE transform IN PIPELINE api_revenue_sync ORDER 2;
```

## CREATE TASK

```sql
CREATE TASK load_raw IN STAGE ingestion IN PIPELINE api_revenue_sync
    TYPE BUILT_IN;

CREATE TASK run_sql IN STAGE transform IN PIPELINE api_revenue_sync
    TYPE SQL
    BODY 'INSERT INTO summary SELECT count(*) FROM raw';

CREATE TASK task_b IN STAGE ingestion IN PIPELINE api_revenue_sync
    TYPE BUILT_IN
    DEPENDS ON task_a;
```

Task types: `SQL`, `BUILT_IN`, others as registered.

`BODY` holds SQL text or script reference (single-quoted string).

## CREATE TRIGGER

```sql
CREATE TRIGGER nightly ON PIPELINE api_revenue_sync
    SCHEDULE '0 2 * * *';
```

Cron-style schedule string in `SCHEDULE`.

## Pipeline control

```sql
RUN PIPELINE api_revenue_sync;
PAUSE PIPELINE api_revenue_sync;
RESUME PIPELINE api_revenue_sync;
```

## ALTER PIPELINE

```sql
ALTER PIPELINE api_revenue_sync SET OWNER 'platform-team';
```

## SHOW

```sql
SHOW PIPELINES;
SHOW STAGES FROM PIPELINE api_revenue_sync;
SHOW TASKS FROM PIPELINE api_revenue_sync;
SHOW TRIGGERS FROM PIPELINE api_revenue_sync;
SHOW PIPELINE RUNS FOR PIPELINE api_revenue_sync;
SHOW PIPELINE METRICS FOR PIPELINE api_revenue_sync;
```

## DROP

```sql
DROP PIPELINE api_revenue_sync;
DROP STAGE ingestion FROM PIPELINE api_revenue_sync;
DROP TASK load_raw FROM STAGE ingestion IN PIPELINE api_revenue_sync;
DROP TRIGGER nightly FROM PIPELINE api_revenue_sync;
```

## DESCRIBE

```sql
DESCRIBE PIPELINE api_revenue_sync;
```

## Multistep ETL pattern

Integration tests build pipelines that:

1. Create tables and views in SQL tasks
2. Load from connectors
3. Refresh materialized views
4. Validate row counts

See `scripts/test_pipelines_api.py`.

## Dependency cycles

`DEPENDS ON` must not form cycles; `RUN PIPELINE` fails when dependencies are cyclic.
