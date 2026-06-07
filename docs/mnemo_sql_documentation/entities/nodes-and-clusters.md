# Nodes and Clusters

Distributed Mnemo deployments register **nodes** (individual server instances) and **clusters** (logical groupings). Every running server bootstraps a **self** node; additional workers register via SQL or HTTP.

## NODE

### Purpose

- Track cluster membership, roles, and health
- Route shards and replicas to capable nodes
- Expose resource metrics for scheduling and observability

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `node_id` | string | Internal unique ID |
| `node_name` | string | Catalog name (`worker_01`) |
| `cluster_id` | string | Parent cluster (empty if unassigned) |
| `node_type` | enum | `COMPUTE`, `STORAGE`, `HYBRID`, `GPU` |
| `node_role` | enum | `COORDINATOR`, `WORKER`, `OBSERVER` |
| `status` | enum | `REGISTERING`, `ONLINE`, `BUSY`, `DEGRADED`, `OFFLINE`, `MAINTENANCE`, `DRAINING` |
| `host` | string | Network host |
| `port` | uint16 | Network port |
| `version` | string | Server version string |
| `is_self` | bool | True for the local process |
| `cpu_cores` | uint32 | Detected or configured CPU cores |
| `memory_bytes` | uint64 | Total memory |
| `gpu_count` | uint32 | GPU count |
| `storage_bytes` | uint64 | Local storage capacity |
| `cpu_utilization_pct` | float | Current CPU utilization |
| `memory_used_bytes` | uint64 | Memory in use |
| `query_throughput` | uint64 | Queries processed |
| `capabilities` | list | e.g. `query_engine`, `columnar_storage` |
| `partition_ids` | list | Assigned shard partitions |
| `replica_ids` | list | Assigned replica IDs |
| `last_heartbeat_at` | timestamp | Last heartbeat received |

### Capabilities by node type

| Type | Typical capabilities |
|------|---------------------|
| `COMPUTE` | `QUERY_ENGINE`, `PIPELINE_EXECUTION` |
| `STORAGE` | `COLUMNAR_STORAGE`, `BLOCK_STORAGE` |
| `HYBRID` | Query + storage |
| `GPU` | `ML_INFERENCE`, `GPU_COMPUTE` |

## CREATE NODE

```sql
CREATE NODE worker_01 TYPE COMPUTE ROLE WORKER;
CREATE NODE coord_01 TYPE HYBRID ROLE COORDINATOR;
CREATE NODE gpu_01 TYPE GPU ROLE WORKER;
CREATE NODE IF NOT EXISTS worker_02 TYPE STORAGE ROLE WORKER;
```

## REGISTER NODE

Attach network coordinates (alternative to HTTP register API):

```sql
REGISTER NODE worker_02 HOST '127.0.0.1' PORT 9001;
```

After registration, heartbeats update metrics and status.

## ALTER NODE

```sql
ALTER NODE worker_01 SET ROLE WORKER;
ALTER NODE worker_01 SET TYPE COMPUTE;
ALTER NODE worker_02 SET CLUSTER production;
```

## DRAIN and REMOVE

Graceful maintenance workflow:

```sql
DRAIN NODE worker_03;          -- status → DRAINING; stop new work
REMOVE NODE worker_03;           -- remove from catalog
REMOVE NODE IF EXISTS worker_03;
```

## SHOW

```sql
SHOW NODES;
SHOW NODE METRICS;
SHOW NODE METRICS worker_01;
SHOW NODE CAPABILITIES;
SHOW NODE CAPABILITIES worker_01;
SHOW NODE PARTITIONS worker_01;
SHOW NODE REPLICAS worker_01;
```

Every server lists a local **self** node on `SHOW NODES` (typically `TYPE HYBRID`, `ROLE COORDINATOR`, `STATUS ONLINE`).

## DESCRIBE

```sql
DESCRIBE NODE worker_02;
```

Returns `host`, `port`, `cpu_cores`, `memory_bytes`, `capabilities`, `cluster_id`, etc.

## CLUSTER

### Purpose

Group nodes under a deployment boundary (production, staging, development).

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Cluster identifier |
| `created_at` | timestamp | Creation time |

### SQL

```sql
CREATE CLUSTER main;
CREATE CLUSTER IF NOT EXISTS production;

SHOW CLUSTERS;
DESCRIBE CLUSTER production;
```

Assign nodes to a cluster:

```sql
ALTER NODE worker_02 SET CLUSTER production;
```

## Complete Python example

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")

# Self node is always visible
r = client.query("SHOW NODES")
assert "local" in r.body.lower() or "hybrid" in r.body.lower()

# Create and register a worker
client.query("REMOVE NODE IF EXISTS worker_01")
client.query("CREATE NODE worker_01 TYPE COMPUTE ROLE WORKER")
client.query("REGISTER NODE worker_02 HOST '127.0.0.1' PORT 9001")

r = client.query("DESCRIBE NODE worker_02")
assert "127.0.0.1" in r.body
assert "cpu_cores" in r.body.lower()

client.query("CREATE CLUSTER IF NOT EXISTS production")
client.query("ALTER NODE worker_02 SET CLUSTER production")

r = client.query("SHOW CLUSTERS")
assert "production" in r.body.lower()

r = client.query("SHOW NODE METRICS")
assert len(r.json().get("data", [])) >= 1
```

## Operational patterns

### Rolling upgrade

```sql
DRAIN NODE worker_01;
-- wait for in-flight queries to complete
REMOVE NODE worker_01;
-- upgrade binary, restart, re-register
CREATE NODE worker_01 TYPE COMPUTE ROLE WORKER;
REGISTER NODE worker_01 HOST '10.0.0.5' PORT 9001;
```

### GPU inference node

```sql
CREATE NODE ml_gpu_01 TYPE GPU ROLE WORKER;
REGISTER NODE ml_gpu_01 HOST '10.0.0.20' PORT 9010;
ALTER NODE ml_gpu_01 SET CLUSTER production;
```

## Related

- [replication.md](replication.md) — replica placement uses node awareness
- [sharding.md](sharding.md) — shard assignment to nodes
- `scripts/test_nodes_api.py`
