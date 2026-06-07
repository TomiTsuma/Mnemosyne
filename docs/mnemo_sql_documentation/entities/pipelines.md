# Pipelines

Pipelines orchestrate staged, dependent tasks (SQL steps, built-in operations) with optional cron triggers. They are Mnemo's workflow engine for ETL, refreshes, and multi-step analytics.

## Hierarchy

```
PIPELINE
  └── STAGE (ordered by ORDER n)
        └── TASK (TYPE SQL | BUILT_IN, optional DEPENDS ON)
  └── TRIGGER (SCHEDULE cron)
  └── RUN (execution history)
```

## PIPELINE

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Pipeline identifier |
| `status` | enum | `CREATING`, `ACTIVE`, `RUNNING`, `PAUSED`, `FAILED`, `ARCHIVED` |
| `owner` | string | Owner team or user |
| `stages` | list | Ordered `StageEntry` objects |
| `triggers` | list | `TriggerEntry` objects |
| `runs` | list | Historical `PipelineRunEntry` records |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

### CREATE PIPELINE

```sql
CREATE PIPELINE api_revenue_sync OWNER 'analytics';
CREATE PIPELINE IF NOT EXISTS nightly_etl OWNER 'data-team';
```

## STAGE

### Properties

| Property | Description |
|----------|-------------|
| `name` | Stage name within pipeline |
| `order` | Execution order (ascending) |
| `tasks` | Tasks in this stage |

### CREATE STAGE

```sql
CREATE STAGE ingestion IN PIPELINE api_revenue_sync ORDER 1;
CREATE STAGE transform IN PIPELINE api_revenue_sync ORDER 2;
CREATE STAGE validate   IN PIPELINE api_revenue_sync ORDER 3;
```

Stages run in `ORDER` sequence; tasks within a stage respect `DEPENDS ON` DAG ordering.

## TASK

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Task identifier |
| `stage_name` | string | Parent stage |
| `type` | enum | `SQL`, `BUILT_IN` |
| `body` | string | SQL text (for `TYPE SQL`) |
| `depends_on` | list | Task names that must complete first |
| `max_retries` | uint32 | Retry limit |
| `status` | enum | Per-run: `PENDING`, `RUNNING`, `SUCCEEDED`, `FAILED`, `SKIPPED` |

### CREATE TASK

```sql
CREATE TASK load_raw IN STAGE ingestion IN PIPELINE api_revenue_sync
    TYPE BUILT_IN;

CREATE TASK create_tables IN STAGE ingestion IN PIPELINE api_revenue_sync
    TYPE SQL
    BODY 'CREATE TABLE IF NOT EXISTS raw_orders (id Int64, total Float64) ENGINE=Memory';

CREATE TASK aggregate IN STAGE transform IN PIPELINE api_revenue_sync
    TYPE SQL
    BODY 'CREATE MATERIALIZED VIEW IF NOT EXISTS daily_totals AS SELECT sum(total) AS revenue FROM raw_orders'
    DEPENDS ON create_tables;

CREATE TASK task_b IN STAGE ingestion IN PIPELINE api_revenue_sync
    TYPE BUILT_IN
    DEPENDS ON task_a;
```

Task types:

| Type | Use |
|------|-----|
| `SQL` | Execute Mnemo SQL in `BODY` |
| `BUILT_IN` | Registered internal step (placeholder / system task) |

## TRIGGER

### Properties

| Property | Description |
|----------|-------------|
| `name` | Trigger identifier |
| `type` | `MANUAL`, `SCHEDULE` |
| `schedule` | Cron expression |
| `last_fired_at` | Last automatic run |

### CREATE TRIGGER

```sql
CREATE TRIGGER nightly ON PIPELINE api_revenue_sync
    SCHEDULE '0 2 * * *';
```

## Pipeline control

```sql
RUN PIPELINE api_revenue_sync;
PAUSE PIPELINE api_revenue_sync;
RESUME PIPELINE api_revenue_sync;
```

`RUN PIPELINE` executes stages in order, respecting task dependencies. Cyclic `DEPENDS ON` graphs cause failure.

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

### Pipeline run record

| Field | Description |
|-------|-------------|
| `run_id` | Unique run identifier |
| `status` | `PENDING`, `RUNNING`, `SUCCEEDED`, `FAILED` |
| `duration_ms` | Total wall time |
| `task_runs` | Per-task status, error, duration |

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

## Multistep ETL example

Full revenue pipeline from integration tests:

```sql
USE pipeline_test_db;

CREATE PIPELINE multistep_revenue_etl OWNER 'analytics';

-- Stage 1: DDL
CREATE STAGE ddl IN PIPELINE multistep_revenue_etl ORDER 1;
CREATE TASK create_customers IN STAGE ddl IN PIPELINE multistep_revenue_etl
    TYPE SQL
    BODY 'CREATE TABLE IF NOT EXISTS ms_customers (id Int64, region String) ENGINE=Memory';
CREATE TASK create_orders IN STAGE ddl IN PIPELINE multistep_revenue_etl
    TYPE SQL
    BODY 'CREATE TABLE IF NOT EXISTS ms_orders (id Int64, customer_id Int64, total Float64) ENGINE=Memory'
    DEPENDS ON create_customers;

-- Stage 2: Load
CREATE STAGE load IN PIPELINE multistep_revenue_etl ORDER 2;
CREATE TASK seed_customers IN STAGE load IN PIPELINE multistep_revenue_etl
    TYPE SQL
    BODY 'INSERT INTO ms_customers VALUES (1, ''US''), (2, ''EU'')'
    DEPENDS ON create_orders;

-- Stage 3: Transform
CREATE STAGE transform IN PIPELINE multistep_revenue_etl ORDER 3;
CREATE TASK build_view IN STAGE transform IN PIPELINE multistep_revenue_etl
    TYPE SQL
    BODY 'CREATE VIEW IF NOT EXISTS revenue_by_region AS SELECT c.region, sum(o.total) AS revenue FROM ms_orders o JOIN ms_customers c ON o.customer_id = c.id GROUP BY c.region';

-- Stage 4: Validate
CREATE STAGE validate IN PIPELINE multistep_revenue_etl ORDER 4;
CREATE TASK check_rows IN STAGE validate IN PIPELINE multistep_revenue_etl
    TYPE SQL
    BODY 'SELECT count(*) FROM ms_customers';

RUN PIPELINE multistep_revenue_etl;
SHOW PIPELINE RUNS FOR PIPELINE multistep_revenue_etl;
```

## ML + pipeline integration

Refresh feature tables before training:

```sql
CREATE PIPELINE ml_refresh OWNER 'ml-team';
CREATE STAGE features IN PIPELINE ml_refresh ORDER 1;
CREATE TASK rebuild_features IN STAGE features IN PIPELINE ml_refresh
    TYPE SQL
    BODY 'REFRESH MATERIALIZED VIEW ml_feature_snapshot';

CREATE TRIGGER pre_train ON PIPELINE ml_refresh SCHEDULE '0 4 * * 0';

-- After RUN PIPELINE ml_refresh:
RUN TRAINING_JOB churn_training;
```

## Python helper

```python
def create_sql_task(client, name, stage, pipeline, sql, depends_on=()):
    deps = f" DEPENDS ON {' '.join(depends_on)}" if depends_on else ""
    return client.query(
        f"CREATE TASK {name} IN STAGE {stage} IN PIPELINE {pipeline} "
        f"TYPE SQL BODY '{sql}'{deps}"
    )
```

## Error handling

| Issue | Behavior |
|-------|----------|
| Cyclic dependencies | `RUN PIPELINE` fails |
| SQL error in task | Run status `FAILED`; error in `task_runs` |
| Missing pipeline | `RUN PIPELINE missing` → error |

## Related

- [connectors.md](connectors.md) — load external data in SQL tasks
- [data-layer.md](data-layer.md) — views and materialized views
- [../model-layer/workflows.md](../model-layer/workflows.md) — train after ETL
- `scripts/test_pipelines_api.py`
