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
ALTER NODE worker_01 SET STATUS MAINTENANCE;
ALTER NODE worker_02 SET CLUSTER production;
```

`SET STATUS` takes any of the status values above. Heartbeats and the offline sweep also change
a node's status, so a manually set status can be overwritten.

`SET CLUSTER` stores the name without checking that the cluster exists or that the node is free.
To manage membership with those checks, use [`ALTER CLUSTER`](#alter-cluster).

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
| `status` | enum | Derived from the member nodes (see [Cluster status](#cluster-status)) |
| `node_count` | integer | Number of member nodes |
| `nodes` | string | Member node names, sorted, comma-separated (`DESCRIBE` only) |

Membership is stored on the node (its `cluster_id`); a cluster is a name plus the nodes that
point at it. A node belongs to at most one cluster.

### SQL

```sql
CREATE CLUSTER main;
CREATE CLUSTER IF NOT EXISTS production;

SHOW CLUSTERS;                 -- name, status, node_count
DESCRIBE CLUSTER production;   -- name, status, node_count, nodes
DROP CLUSTER production;
DROP CLUSTER IF EXISTS production;
```

`DROP CLUSTER` detaches the member nodes (their `cluster_id` is cleared) and leaves the nodes
themselves registered.

### ALTER CLUSTER {#alter-cluster}

```sql
ALTER CLUSTER production ADD NODE worker_01;
ALTER CLUSTER production ADD NODE worker_02, ADD NODE worker_03;
ALTER CLUSTER production REMOVE NODE worker_03;
```

Steps in one statement are applied all-or-nothing: if one fails, every node keeps the cluster
it had. The statement fails when:

- the cluster or a node does not exist;
- `ADD NODE` names a node that already belongs to a **different** cluster (remove it from that
  cluster first); adding a current member again does nothing;
- `REMOVE NODE` names a node that is not a member of this cluster.

A node whose `cluster_id` names no existing cluster is not a member of anything. The self
node starts with `cluster_id` `default`, so it can join a new cluster straight away, which is
how a single-node cluster is made:

```sql
CREATE CLUSTER dev;
ALTER CLUSTER dev ADD NODE local;
```

### Cluster status {#cluster-status}

A cluster's status is worked out from its members each time it is asked for:

| Status | When |
|--------|------|
| `CREATING` | No member nodes yet |
| `ONLINE` | Every member is `ONLINE` or `BUSY` |
| `OFFLINE` | Every member is `OFFLINE` |
| `DEGRADED` | Anything else, such as one member offline, draining or still registering |

```sql
SHOW CLUSTER STATUS;    -- name, status, node_count, online, offline
SHOW CLUSTER METRICS;
```

`SHOW CLUSTER METRICS` rolls the members up per cluster: `node_count`, `cpu_cores`,
`cpu_utilization_pct` (the average), `memory_bytes`, `memory_used_bytes`, `storage_bytes`,
`storage_used_bytes`, `gpu_count`, `query_throughput`, `pipeline_throughput` and
`stream_throughput`. An empty cluster reports zeros.

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
- `scripts/test_nodes_api.py`, `scripts/test_clusters_api.py`
