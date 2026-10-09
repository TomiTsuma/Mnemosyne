---
description: "Web UI rewrite, persistent default `File` engine with startup recovery, qualified `db.table` names, `DROP DATABASE`, and the data-integrity bugs found while verifying them."
sidebar_label: "Web UI and File Engine Persistence"
sidebar_position: 2
slug: /changelog/091026-web-ui-file-engine-persistence
title: "Web UI, Persistent File Engine, and DROP DATABASE"
doc_type: changelog
---

# Web UI, Persistent File Engine, and DROP DATABASE {#web-ui-file-engine-persistence}

**Date:** 2026-10-09
**Branch:** `feature/mnemo-web-ui`

## Summary {#summary}

Closes out the `feature/mnemo-web-ui` branch. The branch started as a UI rewrite and grew into making tables durable: `CREATE TABLE` without an `ENGINE` clause now creates a `File` table on the `su_default` storage unit (`./data`), tables are rebuilt from disk when the server starts, and databases can be dropped and addressed with qualified `db.table` names.

Verifying the branch end to end exposed several data-integrity bugs, which are fixed here and covered by the new `scripts/test_file_engine_api.py` (29 checks).

---

## Behaviour changes {#behaviour-changes}

### Default engine is now `File` {#default-engine}

`CREATE TABLE` with no `ENGINE` previously produced a `Memory` table. It now produces a `File` table bound to `su_default`. Use `ENGINE=Memory` explicitly for the old behaviour.

### On-disk column format changed (breaking) {#on-disk-format}

`FileStorage` used to write every column as a bare `double[]`, so only numeric data survived. Each column file `<table>/<column>.bin` is now:

- Numeric columns: `[uint64 row_count][T values...]`
- String-like columns: `[uint64 row_count]` followed by `[uint32 length][bytes]` per row

Files written by the previous format cannot be read by this version. Each table directory also gets a `_schema.json` with the ordered column names and types.

### Tables are restored on startup {#startup-recovery}

`bootstrap_file_tables` in `programs/server/main.cpp` scans `./data/<database>/<table>/_schema.json` and re-creates each table through `DatabaseManager`, then calls `FileStorage::sync_row_count`.

### Qualified names and `DROP DATABASE` {#qualified-names-drop-database}

- `CREATE TABLE`, `INSERT INTO` and `SELECT` accept `db.table` (`resolve_storage` in `interpreter_ddl_utils.h`).
- `DROP DATABASE [IF EXISTS] name` is parsed and executed (`do_drop_database`), backed by the new `Context::unregister_database`.

### HTTP server and UI {#http-server-ui}

- `public/index.html` rewritten; `public/favicon.ico` and the Nunito fonts (`public/fonts/`, SIL OFL) are served through the new `HTTPHandler::handle_static_asset`, which rejects `..` in paths.
- `HTTPServer` accepts connections on a thread per client so one idle client cannot block the others. Request handling itself is still serialized behind `handle_mutex_` because `Context` has no internal locking.
- JSON output: control characters are escaped as `\u00XX`, `NaN` and infinity are emitted as `null`, and `Response::error` escapes the message.

---

## Bugs fixed while verifying {#bugs-fixed}

### Dangling reference in `FileStorage::write` {#dangling-reference}

`const auto& col_name = block.column_names()[c];` bound a reference into a temporary `std::vector<std::string>` (`Block::column_names` returns by value). In a Release build the freed memory was reused, so every `INSERT` wrote to a file named `.bin` and `SELECT` failed with `column '' size N does not match block row count 0`. The names are now copied once before the loop.

### Floats truncated to six digits in query output {#float-precision}

`execute_query` wrote floating-point values with `ostream`'s default precision, so `-1.286389` came back as `-1.28639` and aggregates lost precision. JSON and TSV output now use `std::to_chars`, which emits the shortest string that round-trips.

### Startup recovery crashed {#startup-crash}

The bootstrap created a `DatabaseMemory`, which rejects `ENGINE=File`, so the server exited with `DatabaseMemory::create_table: unsupported engine 'File'` as soon as `./data` contained a table. It now uses `DatabaseManager::create_database`, the same catalog `Database` that `CREATE DATABASE` uses.

### `DROP TABLE` and `DROP DATABASE` left the data on disk {#drop-left-data}

Only the storage unit reference was released. A later table with the same name silently picked up the old rows, and with startup recovery a dropped table came back after a restart. `release_table_resources` in `interpreter_drop_query.cpp` now deletes the table directory (and `DROP DATABASE` removes the database directory) as well as releasing the storage unit, shard group and replica group references.

`S3Disk::remove_dir` was a stub that returned `false`. It now lists the objects under the prefix and deletes them, so dropping an S3-backed table cleans up too.

### Storage unit reference leaked when `CREATE TABLE` failed {#reference-leak}

`CREATE TABLE` now writes `_schema.json` to the storage unit. If that write failed (for example an unreachable S3 endpoint) the exception left the unit's reference count incremented, and the unit could never be dropped (`Storage unit is in use`). The reference is now released before the exception propagates.

### `SHOW NODE PARTITIONS` returned the node list {#show-node-partitions}

The lexer emits `KeywordPartitions` for the word `PARTITIONS`, but the parser only accepted `KeywordPartition` or an identifier, so the statement fell through to `SHOW NODES`. This broke the `partition customer_distribution/s0` check in `test_shard_groups_api.py`. The parser now accepts `KeywordPartitions`.

---

## Tests {#tests}

New: `scripts/test_file_engine_api.py` (29 checks) covering the default engine, append semantics, `Float64`/negative/`String` round trips, qualified names, `DROP DATABASE`, and that dropped tables and databases do not resurrect rows.

Updated: `scripts/test_storage_units_api.py` — the S3 check no longer assumes `ENGINE=File` on S3 is rejected (it has been supported since the S3 work); it now asserts that an unreachable S3 unit fails `CREATE TABLE` without leaking a reference.

The scripts expect an already running server. Start it from a scratch directory so `./data` is not created in the repository:

```bash
mkdir -p tmp/run && cp -r public tmp/run/public && cd tmp/run
../../build/bin/Release/mnemosyne_server.exe
python3.11 scripts/test_file_engine_api.py --no-launch
```

Results against a Release build:

| Suite | Result |
|-------|--------|
| `test_file_engine_api.py` | 29/29 |
| `test_use_api.py` | 15/15 |
| `test_views_api.py` | 36/36 |
| `test_storage_units_api.py` | 32/32 |
| `test_shard_groups_api.py` | 47/47 |
| `test_replica_groups_api.py` | 38/38 |
| `test_streams_api.py` | 58/58 |
| `test_pipelines_api.py` | 74/74 |
| `test_connectors_api.py` | 38/38 |
| `test_nodes_api.py` | 39/40 |

The `test_nodes_api.py` failure (`worker_03 gone`) is a test-isolation artifact: the check searches for the substring `worker_03`, which also matches `rg_worker_03` and `sg_worker_03` left behind by the replica and shard suites on the same server instance.

A restart check was also done by hand: stop the server, start it again in the same directory, and confirm previously inserted rows are returned.

---

## Known limitations {#known-limitations}

- An empty database is not persisted, because recovery is driven by `_schema.json` files. A database only survives a restart once it holds a table.
- Only `su_default` (`./data`) is scanned at startup. Tables on other storage units, and views, materialized views, streams, pipelines and models, are not restored.
- `DROP TABLE` still resolves its name against the current database only; `DROP TABLE db.t` is not supported yet.
- Each `INSERT` re-reads and rewrites the whole column file. This is the same cost model as before, but now applies to string columns as well.
- `Context` keeps a flat table-name registry, so two databases that both contain a table with the same name can shadow each other in lookups that do not go through the database.

## Not included in this change {#not-included}

Left uncommitted on purpose because they are unrelated to this work: `.claude/settings.json`, `docker-compose.yml` (image name `ttsuma/mnemo` to `ttsu/mnemo`), `notebooks/SimpleDDL.ipynb`, and `src/main.cpp`. The last one carries a duplicate of the bootstrap code but is not part of any CMake target.
