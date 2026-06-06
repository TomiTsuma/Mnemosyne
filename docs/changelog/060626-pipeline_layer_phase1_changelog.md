# Pipeline Layer Phase 1 — changelog

**Date:** 2026-06-06

## Summary

Implemented Pipeline Layer Phase 1: first-class `PIPELINE`, `STAGE`, `TASK`, and `TRIGGER`
entities with in-process DAG execution, manual and cron scheduling, run history,
pause/resume lifecycle, and Python E2E tests.

Phase 1 is catalog + in-process orchestration only — no distributed execution,
stream/table/webhook triggers, or data-quality rules yet.

## Changes by file

### New module: `src/Pipelines/` (`mnemosyne_pipelines`)

- `pipeline_catalog.h` / `.cpp` — types, parsers, validation
- `pipeline_manager.h` / `.cpp` — CRUD, run history, cycle detection, topological sort
- `cron_matcher.h` / `.cpp` — lightweight 5-field cron matcher
- `pipeline_scheduler.h` / `.cpp` — background schedule evaluation (5s tick)
- `CMakeLists.txt`

### Execution (in `src/Interpreters/` to avoid circular deps)

- `pipeline_executor.h` / `.cpp` — DAG execution; SQL tasks via direct interpreters
- `interpreter_run_pipeline.h` / `.cpp` — `RUN` / `PAUSE` / `RESUME`
- `interpreter_alter_pipeline.h` / `.cpp` — `ALTER PIPELINE SET OWNER`

### Parser / Analyzer / Planner

- Lexer keywords: `PIPELINE`, `STAGE`, `TASK`, `TRIGGER`, `RUN`, `PAUSE`, `RESUME`,
  `SCHEDULE`, `DEPENDS`, `BODY`, `OWNER`, `PIPELINE_RUNS`, `PIPELINE_METRICS`, etc.
- `ast.h` — CREATE/DROP/ALTER/SHOW/DESCRIBE targets; `QueryType::Run/Pause/Resume`
- `parser.cpp` — full pipeline DDL and control statements
- `lexer.cpp` — keyword map refactor (fixes MSVC nested-block limit)
- `query_tree.h`, `analyzer.cpp`, `planner.cpp` — DDL/SHOW wiring

### Interpreters

- `interpreter_create_query.cpp` — create pipeline/stage/task/trigger
- `interpreter_drop_query.cpp` — drop pipeline/stage/task/trigger
- `block_interpreter.cpp` — SHOW/DESCRIBE pipeline entities, metrics, runs

### Server

- `server.cpp` — start/stop `PipelineScheduler` with executor callback
- `http_handler.cpp` — fast-path for RUN/PAUSE/RESUME/ALTER PIPELINE

### Build

- Root `CMakeLists.txt` — `add_subdirectory(src/Pipelines)`
- `src/Interpreters/CMakeLists.txt`, `src/Server/CMakeLists.txt` — link `mnemosyne_pipelines`

### Tests

- `scripts/test_pipelines_api.py` — CRUD, DAG run, pause/resume, metrics, scheduler smoke, errors

## SQL now supported

```sql
CREATE PIPELINE revenue_sync [IF NOT EXISTS] [OWNER 'analytics'];
CREATE STAGE ingestion IN PIPELINE revenue_sync [ORDER 1];
CREATE TASK load_raw IN STAGE ingestion IN PIPELINE revenue_sync
  TYPE SQL BODY 'CREATE TABLE t (id Int64) ENGINE=Memory' [DEPENDS ON other_task];
CREATE TASK noop IN STAGE ingestion IN PIPELINE revenue_sync TYPE BUILT_IN;
CREATE TRIGGER nightly ON PIPELINE revenue_sync SCHEDULE '* * * * *';

RUN PIPELINE revenue_sync;
PAUSE PIPELINE revenue_sync;
RESUME PIPELINE revenue_sync;

SHOW PIPELINES;
SHOW STAGES FROM PIPELINE revenue_sync;
SHOW TASKS FROM PIPELINE revenue_sync;
SHOW TRIGGERS FROM PIPELINE revenue_sync;
SHOW PIPELINE_RUNS [FOR PIPELINE revenue_sync];
SHOW PIPELINE_METRICS [FOR PIPELINE revenue_sync];
DESCRIBE PIPELINE revenue_sync;

ALTER PIPELINE revenue_sync SET OWNER 'new_team';
DROP PIPELINE revenue_sync [IF EXISTS];
DROP STAGE ingestion FROM PIPELINE revenue_sync;
DROP TASK load_raw FROM STAGE ingestion IN PIPELINE revenue_sync;
```

## Verification

```powershell
cmake --build build --config Release --target mnemosyne_server
python scripts/test_pipelines_api.py --server-bin build/bin/Release/mnemosyne_server.exe
```

Expected: **45/45 checks passed**.
