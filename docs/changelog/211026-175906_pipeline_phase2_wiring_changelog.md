---
description: "Pipeline Layer Phase 2: `INSERT ... SELECT`, `DATA_QUALITY_RULE` and `QUALITY` tasks connected to the interpreters."
sidebar_label: "Pipeline Phase 2 Wiring"
sidebar_position: 4
slug: /changelog/211026-pipeline-phase2-wiring
title: "Pipeline Layer Phase 2 — interpreter wiring"
doc_type: changelog
---
# Pipeline Layer Phase 2 — interpreter wiring {#pipeline-layer-phase-2-interpreter-wiring}

**Branch:** `feature/pipeline-connector-tasks`
**Date:** 2026-10-21

## Summary {#summary}

Phase 2 of the Pipeline Layer (per `docs/PRDS/PIPELINE_LAYER_.md`, roadmap Phase 2:
Connectors, Data Quality Rules) had its SQL grammar and a complete `DATA_QUALITY_RULE`
backend written, but **none of the new statements were wired into the interpreter /
dispatch layer**. The server parsed the new syntax, then either threw
`INSERT requires VALUES clause` or fell through to `CREATE: expected DATABASE, TABLE, ...`.
`scripts/test_pipeline_phase2_api.py` scored 73/136.

This change connects the already-built backend and the already-parsed AST. After it,
`test_pipeline_phase2_api.py` passes **136/136** and every other API suite stays green
(no regressions).

## What was missing vs. what already existed {#what-was-missing-vs-what-already-existed}

Already in place (committed or staged on the branch), left untouched:

- Grammar for `INSERT ... SELECT`, `CREATE/SHOW/DESCRIBE/DROP/RUN DATA_QUALITY_RULE`,
  and `TaskType::Quality` (`src/Parsers/parser.cpp`, `src/Parsers/ast.h`).
- The `DATA_QUALITY_RULE` backend: `src/DataQuality/` catalog + singleton manager, and
  `src/Interpreters/data_quality_runner.{h,cpp}` (rule evaluation via generated `SELECT`s).

Missing — the interpreter/dispatch wiring added here:

- **`INSERT INTO t [(cols)] SELECT ...`** (`src/Interpreters/interpreter_insert_query.cpp`).
  Runs the `SELECT` (which may read `FROM CONNECTOR`), then converts each produced value
  to the destination column's declared type. Conversion is strict and branches on the
  source `FieldType` rather than the `Field::as_*` accessors, because those silently
  truncate (`as_int64` on a float) or refuse to parse text: a fractional value into an
  integer column, or non-numeric text into a numeric column, is rejected with a message
  containing "convert". The whole target block is built before any `write`, so a single
  bad value aborts the load with nothing written (all-or-nothing). An explicit column list
  fills the unlisted columns with defaults; a target/source column-count mismatch, an
  unknown target table, and an unknown target column are each rejected.
- **`CREATE DATA_QUALITY_RULE`** (`src/Interpreters/interpreter_create_query.cpp`): builds a
  `RuleEntry` from the parsed clauses and registers it with `DataQualityManager`. `EXPRESSION`
  rules are validated with `DataQualityRunner::validate_expression` so a malformed condition
  is refused at creation.
- **`DROP DATA_QUALITY_RULE`** (`src/Interpreters/interpreter_drop_query.cpp`).
- **`SHOW DATA_QUALITY_RULES` / `DESCRIBE DATA_QUALITY_RULE`** via new
  `DataQualityRunner::execute_show` / `execute_describe`, dispatched from
  `src/Server/http_handler.cpp` (other `SHOW`/`DESCRIBE` variants still fall through to the
  planner path).
- **`RUN DATA_QUALITY_RULE`**: routed in `http_handler.cpp` to
  `DataQualityRunner::execute_run`.
- **Pipeline `TYPE QUALITY` tasks** (`src/Interpreters/pipeline_executor.cpp`): a QUALITY task
  evaluates the rules named in its body. A `FAIL`-action rule with any violation aborts the
  task and fails the run; a `WARN` rule records a note on the task but lets the pipeline
  continue. QUALITY tasks are also validated at creation time (rules must exist and the body
  must name at least one) in `do_create_task`.

## Files changed {#files-changed}

- `src/Interpreters/interpreter_insert_query.cpp` — `INSERT ... SELECT` + typed conversion.
- `src/Interpreters/interpreter_create_query.cpp` — `CREATE DATA_QUALITY_RULE`, QUALITY-task
  validation.
- `src/Interpreters/interpreter_drop_query.cpp` — `DROP DATA_QUALITY_RULE`.
- `src/Interpreters/data_quality_runner.{h,cpp}` — `execute_show` / `execute_describe`.
- `src/Interpreters/pipeline_executor.cpp` — QUALITY task execution.
- `src/Server/http_handler.cpp` — `RUN` / `SHOW` / `DESCRIBE` dispatch for rules.

No backend, parser, or AST files were modified by this change.

## Verification {#verification}

- Build: `cmake --build build --config Release --target mnemosyne_server` — clean, 0 warnings.
- `python3.11 scripts/test_pipeline_phase2_api.py --no-launch` — **136/136**.
- Regression sweep (all green): `select` 74, `pipelines` 74, `streams` 58, `models` 54,
  `shard_groups` 47, `nodes` 40, `connectors` 38, `replica_groups` 38, `views` 36,
  `storage_units` 32, `file_engine` 29, `feature_sets` 28, `tuning` 27, `use` 15,
  `replica_groups_failover` 14, `ml_runtime` 12.
