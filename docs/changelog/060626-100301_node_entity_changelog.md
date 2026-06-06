# NODE first-class entity — changelog

**Date:** 2026-06-06
**Spec:** `docs/PRDS/NODE_.md`

## Summary

Implemented the infrastructure NODE first-class entity end-to-end: catalog and
manager, full SQL surface, HTTP-based membership and heartbeats, multi-process
cluster control plane, coordination bridge, distributed execution hooks, and
metadata hooks for partitions/replicas/GPU scheduling. The work follows the
proven STORAGE_UNIT vertical slice (parser → analyzer → planner → interpreter →
HTTP API) and wires a self-node bootstrap plus background heartbeat monitoring
on server start.

Infrastructure `CREATE NODE` is distinct from graph `CREATE NODE_TYPE` in
`docs/PRDS/FIRST_CLASS_ENTITIES.md`; only infrastructure NODE is in scope here.
gRPC/TLS control plane is deferred; HTTP + JSON is the inter-process protocol.

## Changes by file

### New module: `src/Nodes/` (`mnemosyne_nodes`)
- `node_catalog.h` / `node_catalog.cpp`
  - `NodeType` (COMPUTE, STORAGE, HYBRID, GPU), `NodeRole` (COORDINATOR, WORKER,
    OBSERVER), `NodeStatus` (REGISTERING … DRAINING), `NodeEntry`,
    `NodeResources`, `NodeMetrics`, capability helpers, `detect_local_resources()`.
- `node_manager.h` / `node_manager.cpp`
  - Singleton registry: `bootstrap_self`, `create_node`, `register_node`,
    `alter_node`, `drain_node`, `remove_node`, `record_heartbeat`,
    `sweep_stale_nodes`, cluster CRUD, partition/replica assignment metadata,
    `increment_query_throughput`.
- `cluster_catalog.h`
  - Minimal `ClusterEntry` for `CREATE CLUSTER` / `SHOW CLUSTERS`.
- `membership_service.h` / `membership_service.cpp`
  - Background TTL sweep (5 s interval, 15 s default TTL); transitions stale
    nodes `ONLINE → OFFLINE`.
- `node_scheduler.h` / `node_scheduler.cpp`
  - `pick_node`, `pick_gpu_node`, `pick_remote_node` using capabilities, status,
    and resource availability.
- `remote_executor.h` / `remote_executor.cpp`
  - HTTP `POST /query` to peer nodes for sub-plan / SQL fragment execution.
- `CMakeLists.txt`
  - Library target `mnemosyne_nodes`.

### Parser
- `src/Parsers/lexer.h` / `lexer.cpp`
  - Keywords: `NODE`, `NODES`, `REGISTER`, `DRAIN`, `REMOVE`, `HOST`, `PORT`,
    `ROLE`, `METRICS`, `CAPABILITIES`, `COORDINATOR`, `WORKER`, `OBSERVER`,
    `COMPUTE`, `STORAGE`, `HYBRID`, `GPU`, `CLUSTER`, `CLUSTERS`, etc.
- `src/Parsers/ast.h`
  - `Create::Kind::Node`, `Create::Kind::Cluster`; `RegisterNode`, `DrainNode`,
    `RemoveNode`; `QueryType::REGISTER`, `DRAIN`, `REMOVE`; new `ShowType` values
    for nodes, clusters, partitions, replicas; `ObjectKind::Node`.
- `src/Parsers/parser.h` / `parser.cpp`
  - `CREATE NODE`, `CREATE CLUSTER`, `REGISTER NODE … HOST … PORT …`,
    `DRAIN NODE`, `REMOVE NODE`, `SHOW NODES` / `SHOW NODE METRICS|CAPABILITIES|
    PARTITIONS|REPLICAS`, `SHOW CLUSTERS`, `DESCRIBE NODE`, `ALTER NODE … SET
    ROLE|TYPE|STATUS|CLUSTER`.
  - Added `parse_name_or_keyword()` so enum values like `COMPUTE` and `WORKER`
    parse as identifiers in node DDL.
- `src/Parsers/format.cpp`
  - Formatting for all new statement and show types.

### Analyzer / Planner
- `src/Analyzer/query_tree.h` / `analyzer.cpp`
  - DDL show kinds for nodes, clusters, metrics, capabilities, partitions,
    replicas; `REGISTER` / `DRAIN` / `REMOVE` analysis cases.
- `src/Planner/execution_plan.h` / `planner.cpp`
  - `PlanNode::Type::EXCHANGE` for distributed execution; planner emits EXCHANGE
    when multiple online nodes are present.

### Interpreters (new / extended)
- `interpreter_node_query.h` / `.cpp` — `REGISTER`, `DRAIN`, `REMOVE NODE`.
- `interpreter_alter_node.h` / `.cpp` — `ALTER NODE`.
- `interpreter_create_query.cpp` — `do_create_node()`, `do_create_cluster()`.
- `block_interpreter.cpp` — SHOW/DESCRIBE for nodes and clusters; EXCHANGE plan
  node delegates to `RemoteExecutor`.

### Server / HTTP
- `src/Server/http_handler.cpp`
  - Dispatch `REGISTER`, `DRAIN`, `REMOVE` in the direct-AST path.
  - `GET /nodes`, `POST /nodes/register`, `POST /nodes/heartbeat`.
  - `/status` enriched with `node_id`, `role`, `status`, `cluster_id`.
- `src/Server/http_server.cpp`
  - Fixed POST body reading via `Content-Length` (JSON register/heartbeat bodies
    were previously truncated).
- `src/Server/server.cpp`
  - `NodeManager::bootstrap_self("local", …)` and `MembershipService::start()` on
    server boot; `MembershipService::stop()` on shutdown.
- `programs/server/main.cpp`
  - Optional HTTP port CLI arg: `mnemosyne_server [http_port]`.

### Coordination bridge
- `src/Coordination/coordination.cpp`
  - `get_nodes()`, `add_node()`, `remove_node()`, `try_elect_leader()` now read
    and write through `NodeManager` instead of an in-memory stub list.

### Build
- Root `CMakeLists.txt` — `add_subdirectory(src/Nodes)`.
- Linked `mnemosyne_nodes` from Interpreters, Server, Planner, Coordination, and
  `programs/server`.

### Tests
- `scripts/test_nodes_api.py`
  - Full SQL lifecycle, HTTP register/heartbeat, partitions/replicas hooks,
    error cases; optional `--heartbeat-ttl-test` for OFFLINE detection.
- `scripts/test_nodes_distributed_api.py`
  - Two-process coordinator/worker tests with `--launch-worker`.
- `scripts/_mnemo_client.py`
  - Restored `ServerProc` server launch for E2E scripts.

## SQL now supported

```sql
CREATE NODE worker_01 TYPE COMPUTE ROLE WORKER;
CREATE CLUSTER production;
REGISTER NODE worker_02 HOST '127.0.0.1' PORT 9001;
SHOW NODES;
SHOW NODE METRICS;
SHOW NODE CAPABILITIES;
SHOW NODE PARTITIONS;
SHOW NODE REPLICAS;
SHOW CLUSTERS;
DESCRIBE NODE worker_02;
ALTER NODE worker_02 SET ROLE WORKER;
ALTER NODE worker_02 SET CLUSTER production;
DRAIN NODE worker_02;
REMOVE NODE worker_02;
```

## HTTP control plane

| Route | Method | Purpose |
|-------|--------|---------|
| `/nodes` | GET | JSON node list |
| `/nodes/register` | POST | Register or update a node |
| `/nodes/heartbeat` | POST | Liveness + resource metrics |
| `/status` | GET | Enriched with node identity and cluster |

## Verification

```
cmake --build build --config Release --target mnemosyne_server
python scripts/test_nodes_api.py --server-bin build/bin/Release/mnemosyne_server.exe
python scripts/test_nodes_api.py --server-bin build/bin/Release/mnemosyne_server.exe --heartbeat-ttl-test
python scripts/test_nodes_distributed_api.py --launch-worker --server-bin build/bin/Release/mnemosyne_server.exe
```

Result at time of writing:

- `test_nodes_api.py` — **40/40** checks passed
- `test_nodes_api.py --heartbeat-ttl-test` — **41/41** checks passed
- `test_nodes_distributed_api.py --launch-worker` — **9/9** checks passed

## Scope boundaries (deferred / metadata-only)

- **gRPC/TLS** control plane not added; HTTP + JSON ships first per plan.
- **STREAM / PIPELINE / MODEL** execution layers are not implemented; NODE exposes
  registration, scheduling inputs, and metadata hooks (`partition_ids`,
  `replica_ids`, GPU capabilities) for when those entities land.
- **Leader election** is deterministic v1 (coordinator among ONLINE nodes); no
  consensus protocol yet.
- **REPLICA_GROUP / SHARD_GROUP** catalogs are metadata-only on `NodeEntry`; full
  shard/replica routing awaits separate entity work.
