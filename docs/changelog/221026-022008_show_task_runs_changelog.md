---
description: "SHOW TASK RUNS exposes per-task results of pipeline runs, failed runs record the tasks they never reached as SKIPPED, and SHOW PIPELINE RUNS no longer silently lists pipelines."
sidebar_label: "SHOW TASK RUNS"
sidebar_position: 11
slug: /changelog/221026-show-task-runs
title: "SHOW TASK RUNS and Pipeline Run History"
doc_type: changelog
---

# SHOW TASK RUNS and Pipeline Run History {#show-task-runs}

**Branch:** `feature/show-task-runs`
**Date:** 2026-10-22

## Summary {#summary}

After a pipeline run failed, nothing in SQL said which task failed or why. Each run recorded
per-task status, error and duration, but `RUN PIPELINE` returned only `run_id`, `status` and
`duration_ms`, and `SHOW PIPELINE RUNS` added only the pipeline and trigger names. The same gap
hid data quality results: a `FAIL` rule failing its task was visible only on the rule.

This adds `SHOW TASK RUNS`, records the tasks a failed run never reached, and fixes a parsing
bug found while testing it. New `scripts/test_task_runs_api.py` has 82 checks; all 20 other API
suites pass and `mnemosyne_tests` shows the same 4 documented failures.

## Added {#added}

- **`SHOW TASK RUNS [FOR PIPELINE p]`** (also `SHOW TASK_RUNS`): one row per task per run,
  oldest run first, in execution order. Columns: `run_id`, `pipeline`, `stage`, `task`,
  `status`, `duration_ms`, `started_at` (UTC), `error`, `detail`. Without `FOR PIPELINE` it
  covers every pipeline; an unknown pipeline is an error.
- **`SKIPPED` task records.** A run stops at its first failed task. It now records every task it
  did not reach as `SKIPPED`, with `detail` naming the task that failed, so the history accounts
  for every task. `TaskStatus::Skipped` already existed in the catalog and the docs but was never
  used.
- `error` holds the cause of a failed task (an SQL error, `Built-in task forced failure`, or
  `Data quality rule failed: <rule> (<n> violations)`); `detail` on a succeeded task holds the
  data quality rules that only warned.

## Fixed {#fixed}

`SHOW PIPELINE RUNS` (two words, as written in the documentation) returned the list of pipelines
instead of runs, and so did `SHOW PIPELINE` followed by any other word, because the parser fell
through to `SHOW PIPELINES`. Only the one-word `SHOW PIPELINE_RUNS` worked, which is what the
tests and the PRD use, so nothing noticed. It is the same fall-through as the earlier
`SHOW NODE PARTITIONS` bug. `RUNS` is now accepted and an unrecognised word is a syntax error
(`expected RUNS or METRICS after SHOW PIPELINE`); a bare `SHOW PIPELINE` still lists pipelines.

## Checked and left alone {#checked}

A task that throws a plain standard exception rather than `common::Exception` was suspected of
leaving its run stuck in `RUNNING`, which would also stop the scheduler from ever firing that
pipeline again. It does not: the run finishes `FAILED` and can be run again.

## Not done {#not-done}

- `SHOW TASK RUNS` has no filter for a single run or a task; read the rows or add a `WHERE`
  once `SHOW` supports one.
- Task rows are kept in memory with the run, like the rest of the pipeline catalog, and are not
  trimmed, so a long-lived scheduled pipeline accumulates them.
- Retry attempts are not recorded separately; `duration_ms` covers all attempts of a task.

## Files {#files}

- `src/Parsers/ast.h`, `parser.cpp` — `SHOW TASK RUNS`; the `SHOW PIPELINE` fix.
- `src/Analyzer/query_tree.h`, `analyzer.cpp`, `src/Planner/planner.cpp` — plumbing.
- `src/Interpreters/block_interpreter.cpp` — the result; `pipeline_executor.cpp` — `SKIPPED`
  records.
- `scripts/test_task_runs_api.py` (new); docs: `entities/pipelines.md`,
  `metadata/show-and-describe.md`.

## Tests {#tests}

`python3.11 scripts/test_task_runs_api.py --no-launch` — **82/82**: a successful run, a failed
run (which task, why, the rest `SKIPPED`, and that the skipped task did not run), a failing SQL
task, a `WARN` and a `FAIL` data quality rule, all pipelines versus one, the spellings, error
cases, and the `SHOW PIPELINE` spellings.

All other API suites pass: `pipelines` 74, `pipeline_phase2` 136, `export` 71, `alter` 133,
`clusters` 97, `connectors` 38, `select` 74, `views` 36, `storage_units` 32, `use` 15,
`streams` 58, `nodes` 40, `replica_groups` 38, `replica_groups_failover` 14, `shard_groups` 47,
`models` 54, `feature_sets` 28, `tuning` 27, `file_engine` 29, `ml_runtime` 12.
`mnemosyne_tests` (excluding `Coordination creates cluster`): 53 cases, 48 passed, 4 failed,
1 skipped, the same four documented failures.
