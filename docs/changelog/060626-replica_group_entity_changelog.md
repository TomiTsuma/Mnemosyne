# REPLICA_GROUP first-class entity — changelog

**Date:** 2026-06-06
**Spec:** `docs/PRDS/REPLICA_GROUP_.md`

## Summary

Implemented REPLICA_GROUP Phase 1: catalog and manager, full SQL surface, primary-replica
placement on cluster nodes, automatic failover metadata on node OFFLINE transitions,
replication lag monitoring, table attachment with reference counting, background
failover watcher, and Python E2E test scripts.

Phase 1 is policy + placement + failover metadata only — no physical block replication
or quorum write protocol yet.

## Changes by file

### New module: `src/ReplicaGroups/` (`mnemosyne_replica_groups`)

- `replica_group_catalog.h` / `.cpp` — types, parsers, validation
- `replica_group_manager.h` / `.cpp` — CRUD, placement, ref-count, lag, failover hooks
- `replica_failover_service.h` / `.cpp` — background sweep (5s interval)
- `CMakeLists.txt`

### Parser / Analyzer / Planner

- Lexer keywords for REPLICA_GROUP, REPLICATION_STATUS, CONSISTENCY, etc.
- `ast.h` — `ObjectKind::ReplicaGroup`, CREATE/DROP/SHOW/DESCRIBE/ALTER support
- `parser.cpp` — DDL + `CREATE TABLE ... REPLICA_GROUP`
- `format.cpp` — round-trip formatting
- `query_tree.h`, `analyzer.cpp`, `planner.cpp` — SHOW/DESCRIBE wiring

### Interpreters

- `interpreter_create_query.cpp` — `do_create_replica_group()`, table attachment
- `interpreter_drop_query.cpp` — `do_drop_replica_group()`, release on table drop
- `interpreter_alter_replica_group.h` / `.cpp` — `SET REPLICAS`, `SET CONSISTENCY`
- `block_interpreter.cpp` — `SHOW REPLICA_GROUPS`, `SHOW REPLICATION_STATUS`, DESCRIBE

### Storage

- `i_storage.h` — optional `replica_group_name()` / `set_replica_group_name()`
- `memory_storage`, `file_storage` — attachment field

### Server / Nodes integration

- `server.cpp` — start/stop `ReplicaFailoverService`; register offline callback → `on_node_offline`
- `node_manager` / `membership_service` — `sweep_stale_nodes` returns newly offline IDs; invokes callbacks
- `http_handler.cpp` — ALTER REPLICA_GROUP fast-path

### Build

- Root `CMakeLists.txt` — `add_subdirectory(src/ReplicaGroups)`
- `src/Interpreters/CMakeLists.txt`, `src/Server/CMakeLists.txt` — link `mnemosyne_replica_groups`

### Tests

- `scripts/test_replica_groups_api.py` — catalog CRUD, status, table attach, errors
- `scripts/test_replica_groups_failover_api.py` — primary failover on heartbeat TTL

## SQL now supported

```sql
CREATE REPLICA_GROUP standard_ha REPLICAS 3 CONSISTENCY QUORUM;
DROP REPLICA_GROUP [IF EXISTS] standard_ha;
SHOW REPLICA_GROUPS;
SHOW REPLICATION_STATUS;
DESCRIBE REPLICA_GROUP standard_ha;
ALTER REPLICA_GROUP standard_ha SET REPLICAS 5;
ALTER REPLICA_GROUP standard_ha SET CONSISTENCY SYNCHRONOUS;
CREATE TABLE sales (id Float64) ENGINE=Memory REPLICA_GROUP standard_ha;
```

## Verification

```powershell
cmake --build build --config Release --target mnemosyne_server
python scripts/test_replica_groups_api.py --server-bin build/bin/Release/mnemosyne_server.exe
python scripts/test_replica_groups_failover_api.py --server-bin build/bin/Release/mnemosyne_server.exe
```

Result at time of writing:

- `test_replica_groups_api.py` — **38/38** checks passed
- `test_replica_groups_failover_api.py` — **14/14** checks passed

## Scope boundaries (deferred)

- Physical data replication / log shipping
- Quorum write commit protocol (consistency is policy metadata only)
- MULTI_PRIMARY, RACK_AWARE, REGION_AWARE (enum stubs only)
- Stream / index / model attachment
