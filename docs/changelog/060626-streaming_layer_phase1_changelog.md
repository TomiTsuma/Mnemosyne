# Streaming Layer Phase 1 — changelog

**Date:** 2026-06-06

## Summary

Implemented Streaming Layer Phase 1: first-class `STREAM`, `TOPIC`, and `CONSUMER_GROUP`
entities with in-memory event storage, `INSERT`/`PUBLISH`/`SUBSCRIBE`, consumer-group
offset tracking, introspection SQL, and Python E2E tests.

Phase 1 is catalog + in-process in-memory streaming only — no replay, windowing,
stream processors, event triggers, or distributed fan-out yet.

## Changes by file

### New module: `src/Streaming/` (`mnemosyne_streaming`)

- `stream_catalog.h` / `.cpp` — types, enums, validation
- `stream_manager.h` / `.cpp` — singleton CRUD, append/publish/subscribe, metrics
- `CMakeLists.txt`

### Interpreters

- `interpreter_create_query.cpp` — `do_create_stream/topic/consumer_group`
- `interpreter_drop_query.cpp` — drop stream entities
- `interpreter_alter_stream.cpp` — `ALTER STREAM SET RETENTION`
- `interpreter_stream_control.cpp` — `PUBLISH` / `SUBSCRIBE`
- `interpreter_insert_query.cpp` — `INSERT INTO <stream>` routing
- `block_interpreter.cpp` — SHOW/DESCRIBE stream entities and metrics

### Parser / Analyzer / Planner

- Lexer keywords: `STREAM`, `TOPIC`, `CONSUMER_GROUP`, `PUBLISH`, `SUBSCRIBE`, `RETAIN`, etc.
- `ast.h` — object kinds, `StreamControl`, show types
- `parser.cpp` — DDL + publish/subscribe + alter retention
- `analyzer.cpp` — pass-through for publish/subscribe; SHOW mapping
- `query_tree.h`, `planner.cpp` — SHOW/DESCRIBE wiring

### Server

- `http_handler.cpp` — fast-path for PUBLISH/SUBSCRIBE/ALTER STREAM; JSON string escaping fix

### Build

- Root `CMakeLists.txt` — `add_subdirectory(src/Streaming)`
- `src/Interpreters/CMakeLists.txt` — link `mnemosyne_streaming`

### Tests

- `scripts/test_streams_api.py` — 58 checks (CRUD, events, consumer groups, metrics, errors)

## SQL now supported

```sql
CREATE STREAM user_events [IF NOT EXISTS] [TOPIC user_activity] [RETAIN 7 DAYS];
CREATE TOPIC user_activity [IF NOT EXISTS] [PARTITIONS 4] [RETAIN 30 DAYS];
CREATE CONSUMER_GROUP analytics_service [IF NOT EXISTS];

INSERT INTO user_events VALUES ('{"user_id":1,"action":"click"}');
PUBLISH user_activity VALUES ('{"user_id":1,"action":"click"}');
SUBSCRIBE user_events [CONSUMER_GROUP analytics_service] [LIMIT 10];

ALTER STREAM user_events SET RETENTION 14 DAYS;

SHOW STREAMS;
SHOW TOPICS;
SHOW CONSUMER_GROUPS;
SHOW STREAM_METRICS [FOR STREAM user_events];
DESCRIBE STREAM user_events;
DESCRIBE TOPIC user_activity;
DESCRIBE CONSUMER_GROUP analytics_service;

DROP STREAM|TOPIC|CONSUMER_GROUP name [IF EXISTS];
```

## Verification

```bash
cmake --build build --config Release --target mnemosyne_server
python scripts/test_streams_api.py --server-bin build/bin/Release/mnemosyne_server.exe
```
