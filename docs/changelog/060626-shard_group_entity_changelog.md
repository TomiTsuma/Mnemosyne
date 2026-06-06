# SHARD_GROUP first-class entity — changelog

**Date:** 2026-06-06
**Spec:** `docs/PRDS/SHARD_GROUP_.md`

## Summary

Implemented SHARD_GROUP Phase 1: catalog and manager, full SQL surface, static HASH
sharding policy metadata, round-robin shard placement on cluster nodes via
`NodeManager::assign_partition`, table attachment with reference counting, and Python
E2E test script.

Phase 1 is policy + placement metadata only — no physical hash routing, rebalancing,
or distributed query execution yet.

## Changes by file

### New module: `src/ShardGroups/` (`mnemosyne_shard_groups`)

- `shard_group_catalog.h` / `.cpp` — types, parsers, validation
- `shard_group_manager.h` / `.cpp` — CRUD, placement, ref-count
- `CMakeLists.txt`

### Parser / Analyzer / Planner

- Lexer keywords for SHARD_GROUP, SHARD_GROUPS, SHARDS, KEY, etc.
- `ast.h` — `ObjectKind::ShardGroup`, CREATE/DROP/SHOW/DESCRIBE/ALTER support
- `parser.cpp` — DDL + `CREATE TABLE ... SHARD_GROUP`; fix `SHOW NODE PARTITIONS` plural
- `format.cpp` — round-trip formatting
- `query_tree.h`, `analyzer.cpp`, `planner.cpp` — SHOW/DESCRIBE wiring

### Interpreters

- `interpreter_create_query.cpp` — `do_create_shard_group()`, table attachment
- `interpreter_drop_query.cpp` — `do_drop_shard_group()`, release on table drop
- `interpreter_alter_shard_group.h` / `.cpp` — `SET SHARDS`
- `block_interpreter.cpp` — `SHOW SHARD_GROUPS`, `SHOW SHARDS`, `SHOW SHARD_STATUS`, DESCRIBE

### Storage

- `i_storage.h` — optional `shard_group_name()` / `set_shard_group_name()`
- `memory_storage`, `file_storage` — attachment field

### Server

- `http_handler.cpp` — ALTER SHARD_GROUP fast-path

### Build

- Root `CMakeLists.txt` — `add_subdirectory(src/ShardGroups)`
- `src/Interpreters/CMakeLists.txt`, `src/Server/CMakeLists.txt` — link `mnemosyne_shard_groups`

### Tests

- `scripts/test_shard_groups_api.py` — catalog CRUD, status, table attach, combined
  SHARD_GROUP + REPLICA_GROUP, errors

## SQL now supported

```sql
CREATE SHARD_GROUP customer_distribution TYPE HASH KEY customer_id SHARDS 16;
DROP SHARD_GROUP [IF EXISTS] customer_distribution;
SHOW SHARD_GROUPS;
SHOW SHARDS;
SHOW SHARD_STATUS;
DESCRIBE SHARD_GROUP customer_distribution;
ALTER SHARD_GROUP customer_distribution SET SHARDS 32;
CREATE TABLE customers (id Float64, customer_id Float64) ENGINE=Memory SHARD_GROUP customer_distribution;
CREATE TABLE sales (id Float64) ENGINE=Memory SHARD_GROUP customer_distribution REPLICA_GROUP standard_ha;
```

## Verification

```powershell
cmake --build build --config Release --target mnemosyne_server
python scripts/test_shard_groups_api.py --server-bin build/bin/Release/mnemosyne_server.exe
```

Result at time of writing:

- `test_shard_groups_api.py` — **47/47** checks passed

## Scope boundaries (deferred)

- `REBALANCE SHARD_GROUP`
- RANGE / LIST / COMPOSITE strategies (enum stubs only)
- `hash(key) % shard_count` on INSERT/SELECT
- `RemoteExecutor` fan-out + planner `EXCHANGE`
- Hot-shard detection, split/merge
- Stream / index / MV attachment
- Physical data movement between nodes
