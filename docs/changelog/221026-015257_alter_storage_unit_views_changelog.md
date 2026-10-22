# ALTER STORAGE_UNIT, ALTER VIEW, ALTER MATERIALIZED VIEW and scheduled refresh

**Branch:** `feature/alter-storage-unit-and-views`
**Date:** 2026-10-22

## Summary

`docs/PRDS/STORAGE_UNIT.md`, `VIEW_.md` and `MATERIALIZED_VIEW_.md` each specify an `ALTER`
statement that was not parsed:

```sql
ALTER STORAGE_UNIT warehouse SET RETENTION 365 DAYS;
ALTER VIEW active_customers AS SELECT ...;
ALTER MATERIALIZED VIEW monthly_revenue SET REFRESH EVERY 1 HOUR;
```

All three now work, and `SET REFRESH EVERY` is backed by a real scheduler (the PRD lists
scheduled refresh as Materialized View Phase 2). New `scripts/test_alter_api.py` has 133 checks.
All 18 other API suites still pass, and `mnemosyne_tests` shows the same 4 documented failures.

## What was added

- **`ALTER STORAGE_UNIT <name> SET RETENTION <n> [DAY|DAYS] | FOREVER`.** The policy is stored
  on the unit and shown by `SHOW STORAGE_UNITS` (`retention` column) and `DESCRIBE
  STORAGE_UNIT` (`retention`, `retention_enforced`). **It is recorded only: nothing expires
  data yet, and `DESCRIBE` says `retention_enforced = false`.** Tables carry no per-row age, so
  enforcing it would mean deleting data on a guess; that needs its own design. `0 DAYS`, a
  missing unit, an unknown property and a count that does not fit are rejected.
- **`ALTER VIEW <name> AS SELECT ...`.** Replaces the definition and dependency list. It is
  refused, leaving the old definition in place, when:
  - the name is not a view (the message says what it is instead);
  - a referenced table or view does not exist, or the SELECT fails the analyzer;
  - the new definition would read the view itself, directly or through other views;
  - the new definition does not run, or a view that reads this one (directly or transitively)
    would stop running.
- **`ALTER MATERIALIZED VIEW <name> SET REFRESH EVERY <n> <unit> | MANUAL`**, with units
  `SECOND(S)`, `MINUTE(S)`, `HOUR(S)`, `DAY(S)`. The ALTER changes the policy only; it does not
  refresh the data. `SHOW MATERIALIZED VIEWS` gained `status`, `refresh`,
  `refresh_interval_seconds`, `last_refreshed_at` and `last_error` after `name`. `DESCRIBE
  MATERIALIZED VIEW` is unchanged (it lists the backing table's columns).
- **`MaterializedViewScheduler`**, started and stopped with the server. Once a second it
  refreshes every scheduled view, in every database, whose interval has passed since its last
  refresh *attempt*. A failing refresh is logged, leaves the view `DEGRADED` with `last_error`,
  keeps the last good data, and is retried one interval later; it never stops the scheduler.
  Manual `REFRESH` now records its attempt too, and a view's first schedule counts from when it
  was populated.

## Decisions worth knowing about

- **`ALTER VIEW` validates by running the definitions.** After swapping the definition in, it
  executes the new one and every view that reads it, and restores the old definition if any
  fails. The analyzer cannot do this job: when it analyses a SELECT through a view it exposes
  every column of the underlying tables, so it accepts a column the view does not output, and
  that only fails at execution (`Unknown column`). The consequence is that `ALTER VIEW` reads
  the views' sources once. A cheap analyzer pre-check still gives early, specific errors.
  Not fixed: `CREATE VIEW` still accepts a definition that reads such a column.
- **One lock for everything that touches the shared `Context`.** The HTTP server serialised
  requests with a private mutex because `Context` and the catalogs have no locking. That mutex
  is now `common::execution_mutex()` (`Common/execution_lock.h`) and the scheduler takes it for
  each refresh, so a refresh can never overlap a request. The scheduler points the `Context` at
  the view's database for the refresh and restores the client's current database afterwards
  (RAII), so `USE` is not disturbed.
- **Schedule is measured from the last attempt, not the last success**, so a view whose source
  was dropped is retried at its interval rather than on every tick.
- **Spelling.** The grammar uses the two-word `MATERIALIZED VIEW` everywhere (`CREATE`,
  `REFRESH`, `DESCRIBE`, `DROP`), so `ALTER` does too. The PRD's `MATERIALIZED_VIEW` is accepted
  by none of them; that is a separate, cross-cutting grammar change.

## Not changed, but found

- **`PipelineScheduler` does not take the execution lock.** Scheduled pipeline runs execute on
  the scheduler thread against the same shared `Context` as request threads, so they can race
  with requests. Pre-existing; left alone because holding the lock for a whole pipeline run
  changes behaviour. The fix is to take `common::execution_mutex()` in the run callback in
  `Server::start`.
- A refresh of a non-memory materialized view truncates its table before writing, so a failure
  between the two loses the data. Pre-existing; the scheduler makes it more likely to be hit.
- `Context` caches storages by bare table name, not by database. A refresh in another database
  registers its tables there, which is what `USE other; REFRESH ...; USE first` does today.
- Materialized views that read an altered view are not checked: they hold materialized data and
  surface a break on their next refresh (`DEGRADED`, `last_error`).

## Not done (still open in the three PRDs)

Incremental, event-driven and continuous refresh; query rewrite to materialized views;
adaptive refresh; retention enforcement, tiering policies, snapshots and `ALTER` properties
other than `RETENTION` for storage units; view lineage, security and parameterised views.

## Files changed

- `src/Common/execution_lock.h`, `time_format.h` (new).
- `src/Server/http_server.{h,cpp}` (shared lock), `server.cpp` (start/stop the scheduler),
  `http_handler.cpp` (dispatch).
- `src/StorageUnits/storage_unit_catalog.{h,cpp}`, `storage_unit_manager.{h,cpp}` — retention.
- `src/Databases/view_catalog.h`, `database.{h,cpp}` — refresh state, `replace_view`.
- `src/Parsers/ast.h`, `parser.cpp` — the three `ALTER` forms.
- `src/Interpreters/interpreter_alter_storage_unit.{h,cpp}`, `interpreter_alter_view.{h,cpp}`,
  `materialized_view_scheduler.{h,cpp}` (new); `interpreter_refresh_query.cpp`,
  `interpreter_create_query.cpp`, `block_interpreter.cpp` — recording and output.
- `scripts/test_alter_api.py` (new).

## Verification

- Build: `cmake --build build --config Release --target mnemosyne_server` — 0 errors.
- `python3.11 scripts/test_alter_api.py --no-launch` — **133/133**.
- All other API suites pass: `clusters` 97, `pipeline_phase2` 136, `select` 74, `pipelines` 74,
  `streams` 58, `models` 54, `shard_groups` 47, `nodes` 40, `connectors` 38, `replica_groups`
  38, `views` 36, `storage_units` 32, `file_engine` 29, `feature_sets` 28, `tuning` 27, `use` 15,
  `replica_groups_failover` 14, `ml_runtime` 12.
- `mnemosyne_tests` (excluding `Coordination creates cluster`, which kills the process): 53
  cases, 48 passed, 4 failed, 1 skipped — the same four documented failures as before.
