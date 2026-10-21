# Pipelines

Pipelines orchestrate staged, dependent tasks (SQL steps, built-in operations) with optional cron triggers. They are Mnemo's workflow engine for ETL, refreshes, and multi-step analytics.

## Hierarchy

```
PIPELINE
  └── STAGE (ordered by ORDER n)
        └── TASK (TYPE SQL | QUALITY | BUILT_IN, optional DEPENDS ON)
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
| `type` | enum | `SQL`, `QUALITY`, `BUILT_IN` |
| `body` | string | SQL text (`TYPE SQL`) or comma-separated data quality rule names (`TYPE QUALITY`) |
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
| `QUALITY` | Evaluate the data quality rules named in `BODY`; see [Data quality](#data-quality) |
| `BUILT_IN` | Registered internal step (placeholder / system task) |

A `SQL` task can load data with `INSERT INTO … SELECT`, including from a connector:

```sql
CREATE TASK extract IN STAGE s IN PIPELINE orders_etl TYPE SQL
    BODY 'INSERT INTO staging SELECT order_id, customer_id, total FROM CONNECTOR api.orders';
```

See [../dml/README.md](../dml/README.md#insert-select).

## Data quality {#data-quality}

A `DATA_QUALITY_RULE` is a named check on one table. Run it by hand, or put it in a pipeline
with a `QUALITY` task to stop the pipeline when the data is bad.

### CREATE DATA_QUALITY_RULE {#create-data-quality-rule}

```sql
CREATE DATA_QUALITY_RULE uniq_customer ON orders UNIQUE customer_id ACTION WARN;
CREATE DATA_QUALITY_RULE total_nonneg  ON orders RANGE total MIN 0 ACTION FAIL;
CREATE DATA_QUALITY_RULE total_capped  ON orders RANGE total MIN -10 MAX 100;
CREATE DATA_QUALITY_RULE ids_positive  ON orders EXPRESSION 'order_id > 0';
CREATE DATA_QUALITY_RULE has_rows      ON orders MIN_ROWS 4 ACTION FAIL;
CREATE DATA_QUALITY_RULE IF NOT EXISTS has_rows ON orders MIN_ROWS 1;
```

```ebnf
rule   ::= CREATE DATA_QUALITY_RULE [ IF NOT EXISTS ] name ON table check [ ACTION ( WARN | FAIL ) ]
check  ::= UNIQUE column
         | RANGE column [ MIN number ] [ MAX number ]
         | EXPRESSION 'condition'
         | MIN_ROWS integer
```

| Check | A violation is |
|-------|----------------|
| `UNIQUE col` | A row whose `col` value is shared with another row (the count is rows, not distinct values) |
| `RANGE col MIN a MAX b` | A row with `col` below `a` or above `b`; at least one bound is required and `MIN` may not exceed `MAX` |
| `EXPRESSION 'cond'` | A row for which the condition is not true; the condition must be a complete boolean expression |
| `MIN_ROWS n` | The table has fewer than `n` rows; the count is the shortfall |

`ACTION` defaults to `FAIL`. `ACTION QUARANTINE` is recognised but not supported yet and is
rejected. The table name may be database-qualified; column names must be plain identifiers.
A rule is created even if its table or column does not exist yet; running it then fails with an
error naming what is missing, and never reports a pass.

### Run, inspect, drop

```sql
RUN DATA_QUALITY_RULE total_nonneg;
SHOW DATA_QUALITY_RULES;
DESCRIBE DATA_QUALITY_RULE total_nonneg;
DROP DATA_QUALITY_RULE total_nonneg;
DROP DATA_QUALITY_RULE IF EXISTS total_nonneg;
```

`RUN` returns `rule`, `table`, `check`, `action`, `status`, `violations` and `message`.
`status` is `PASSED` (no violations), `WARNED` (violations, action `WARN`) or `FAILED`
(violations, action `FAIL`). A violation is a result, not an error. `SHOW` and `DESCRIBE`
list each rule's definition and its last result as `last_status` (`NOT_RUN` until first run),
`last_violations` and `last_message`. Rules are held in the server's catalog and are not
written to disk.

### QUALITY tasks {#quality-tasks}

```sql
CREATE TASK validate IN STAGE s IN PIPELINE orders_etl
    TYPE QUALITY BODY 'staged_rows, staged_nonneg'
    DEPENDS ON extract;
```

The body names one or more existing rules, separated by commas; a task naming an unknown rule,
or none, is rejected when it is created. When the task runs it evaluates each rule in turn:

- a `FAIL` rule with violations fails the task, which fails the run and stops it: no later task
  runs, dependent or not;
- a `WARN` rule with violations does not stop the pipeline.

Each rule's result is recorded on the rule, so after a run `SHOW DATA_QUALITY_RULES` shows which
rule warned or failed and how many rows. `SHOW TASK RUNS` shows it per task: a failed `QUALITY`
task carries the rule and violation count in `error`, and a task whose rules only warned carries
them in `detail`. See [Task run history](#task-run-history).

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
SHOW TASK RUNS FOR PIPELINE api_revenue_sync;
SHOW PIPELINE METRICS FOR PIPELINE api_revenue_sync;
```

`SHOW PIPELINE_RUNS`, `SHOW PIPELINE_METRICS` and `SHOW TASK_RUNS` are accepted as one-word
spellings. `FOR PIPELINE` is optional; without it the statement covers every pipeline. A word
after `SHOW PIPELINE` other than `RUNS` or `METRICS` is a syntax error.

### Pipeline run record

| Field | Description |
|-------|-------------|
| `run_id` | Unique run identifier |
| `status` | `PENDING`, `RUNNING`, `SUCCEEDED`, `FAILED` |
| `duration_ms` | Total wall time |

`SHOW PIPELINE RUNS` returns `run_id`, `pipeline`, `trigger`, `status` and `duration_ms`: one
row per run. To see what each task did, use `SHOW TASK RUNS`.

### Task run history {#task-run-history}

```sql
SHOW TASK RUNS FOR PIPELINE api_revenue_sync;
```

One row per task per run, oldest run first, in the order the tasks ran:

| Column | Description |
|--------|-------------|
| `run_id`, `pipeline` | The run the row belongs to |
| `stage`, `task` | Where the task is defined |
| `status` | `SUCCEEDED`, `FAILED` or `SKIPPED` |
| `duration_ms` | Wall time of the task, including retries |
| `started_at` | UTC start time; empty for a `SKIPPED` task |
| `error` | Why a `FAILED` task failed |
| `detail` | A note: for a `SUCCEEDED` task, such as the data quality rules that warned; for a `SKIPPED` task, which task failed before it |

A run stops at its first failed task. The tasks it never reached are recorded as `SKIPPED`, so
every task of the run appears. `RUN PIPELINE` on a pipeline whose task dependencies form a cycle
fails with an error before a run is created, so such an attempt has no rows here.

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
| SQL error in task | Run status `FAILED` and the run stops; the error is in `SHOW TASK RUNS` and later tasks are `SKIPPED` |
| `FAIL` data quality rule violated | Run status `FAILED` and the run stops; see [Data quality](#data-quality) |
| Missing pipeline | `RUN PIPELINE missing` → error |

## Related

- [connectors.md](connectors.md) — load external data in SQL tasks
- [data-layer.md](data-layer.md) — views and materialized views
- [../model-layer/workflows.md](../model-layer/workflows.md) — train after ETL
- `scripts/test_pipelines_api.py`, `scripts/test_pipeline_phase2_api.py`
