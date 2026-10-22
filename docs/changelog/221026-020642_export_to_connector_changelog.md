---
description: "`EXPORT TABLE ... TO CONNECTOR` with `APPEND` and `OVERWRITE` modes and a `write` operation on connector drivers."
sidebar_label: "EXPORT to Connector"
sidebar_position: 8
slug: /changelog/221026-export-to-connector
title: "EXPORT TABLE ... TO CONNECTOR"
doc_type: changelog
---
# EXPORT TABLE ... TO CONNECTOR {#export-table-to-connector}

**Branch:** `feature/export-to-connector`
**Date:** 2026-10-22

## Summary {#summary}

`docs/PRDS/CONNECTOR_.md` §11 (Data Loading) specifies

```sql
EXPORT TABLE customers TO CONNECTOR salesforce_prod;
```

Connectors could be read (`SELECT ... FROM CONNECTOR`, `INSERT ... SELECT FROM CONNECTOR`) but
nothing could be written back. This adds the statement and a `write` operation on the connector
driver interface. New `scripts/test_export_api.py` has 71 checks; all 19 other API suites pass
and `mnemosyne_tests` shows the same 4 documented failures.

## Syntax and behaviour {#syntax-and-behaviour}

```sql
EXPORT TABLE <table> TO CONNECTOR <connector>[.<resource>] [MODE APPEND | OVERWRITE]
```

- **Source.** Any table, view or materialized view, optionally database-qualified. It is read
  with `SELECT *`. A materialized view exports what it currently holds, not a fresh run of its
  query, so a view that has not been refreshed exports stale data.
- **Resource.** Defaults to the table's name (without a `db.` prefix), or is given as
  `connector.resource`, the same shape `FROM CONNECTOR` uses.
- **Mode.** `APPEND` is the default because `OVERWRITE` replaces data in the external system and
  should be asked for. `MERGE` and `UPSERT` (the other two modes in the PRD) are recognised and
  refused with "not supported yet": they need a key and a driver that can update in place, and
  faking them would corrupt data. Any other word is an error.
- **Result.** One row: `connector, resource, mode, rows`.
- **`EXPORT` is not a reserved word.** It is matched by text at the start of a statement, so a
  column or table named `export` still works.

## REST driver {#rest-driver}

`RestConnector::write` sends the whole result as a JSON array of row objects to
`<BASE_URL>/<resource>`: `POST` for `APPEND`, `PUT` for `OVERWRITE`, with the connector's API
key header if it has one. Any non-2xx response is an error carrying the status.

- One request per export, so it is all-or-nothing as far as this side goes. There is no
  batching and no size limit, so a very large table is one very large body.
- The body is built before connecting, so a value that cannot be encoded sends nothing.
- Integers and floats are JSON numbers (NaN and infinity become `null`, since JSON has none),
  booleans are `true`/`false`, strings are escaped. Types with no JSON form (arrays) are
  refused naming the column.
- Appending zero rows sends nothing; overwriting with zero rows sends `[]`, which clears the
  resource.

## Not done {#not-done}

- **Postgres and S3 cannot be exported to.** Both drivers are still catalog-only stubs whose
  `read` also throws `NOT_IMPLEMENTED`; `write` does the same. Their capability lists already
  claimed `WRITE` before this change and still do, so the capability check passes and the
  driver then refuses.
- Real third-party REST APIs: the suite runs against a mock REST server that stores what it is
  sent and serves it back, which is what makes the round trip test possible. The JSON array
  shape matches what the connector's own `read` expects, but no real service was exercised.
- `CREATE CDC_SOURCE`, `CREATE STREAM ... SOURCE CONNECTOR` and the SaaS, streaming and AI
  connector categories remain open in the PRD.

## Interface change {#interface-change}

`IConnector` gained a pure virtual `write(entry, resource, block, mode) -> size_t`; all three
drivers implement it. The REST capability list now includes `WRITE`.

## Files changed {#files-changed}

- `src/Connectors/connector_catalog.{h,cpp}` (`ExportMode`, REST `WRITE`), `i_connector.h`,
  `rest_connector.{h,cpp}`, `postgres_connector.{h,cpp}`, `s3_connector.{h,cpp}`.
- `src/Parsers/ast.h`, `parser.{h,cpp}`, `format.cpp`; `src/Analyzer/analyzer.cpp`.
- `src/Interpreters/interpreter_export_query.{h,cpp}` (new); `src/Server/http_handler.cpp`.
- `scripts/test_export_api.py` (new).

## Verification {#verification}

- `python3.11 scripts/test_export_api.py --no-launch` — **71/71**: the request actually sent
  (method, path, content type, API key, typed body with quotes escaped), a round trip back
  through `INSERT ... SELECT FROM CONNECTOR`, empty tables, remote 503, an unreachable server,
  unsupported modes and connector types, and syntax errors.
- All other API suites pass (`connectors` 38, `pipeline_phase2` 136, `alter` 133, `clusters` 97,
  `select` 74, `pipelines` 74, `streams` 58, `models` 54, `shard_groups` 47, `nodes` 40,
  `replica_groups` 38, `views` 36, `storage_units` 32, `file_engine` 29, `feature_sets` 28,
  `tuning` 27, `use` 15, `replica_groups_failover` 14, `ml_runtime` 12).
- `mnemosyne_tests` (excluding `Coordination creates cluster`): 53 cases, 48 passed, 4 failed,
  1 skipped — the same four documented failures.
