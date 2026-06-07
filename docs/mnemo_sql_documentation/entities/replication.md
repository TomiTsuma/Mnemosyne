# Replication (Replica Groups)

`REPLICA_GROUP` defines how many copies of table data exist and how writes are acknowledged. Bind replica groups to tables for high-availability placement.

## Purpose

- Declare replication factor and consistency mode
- Control replica placement policy (node-aware, rack-aware, region-aware)
- Monitor replica health and failover events

## Properties

### ReplicaGroupEntry

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Group identifier |
| `replication_factor` | uint32 | Number of replicas (`REPLICAS n`) |
| `strategy` | enum | `PRIMARY_REPLICA`, `MULTI_PRIMARY`, `OBSERVER` |
| `consistency_mode` | enum | `SYNCHRONOUS`, `ASYNCHRONOUS`, `QUORUM` |
| `placement_policy` | enum | `NODE_AWARE`, `RACK_AWARE`, `REGION_AWARE` |
| `status` | enum | `ONLINE`, `DEGRADED`, `OFFLINE` |
| `reference_count` | size_t | Tables using this group |
| `owner` | string | Optional owner |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

### ReplicaMember (runtime)

| Property | Description |
|----------|-------------|
| `replica_id` | Replica identifier |
| `node_id` | Hosting node |
| `role` | `PRIMARY`, `REPLICA`, `OBSERVER` |
| `state` | `CREATING`, `SYNCING`, `ONLINE`, `DEGRADED`, `OFFLINE`, … |
| `lag_ms` | Replication lag in milliseconds |
| `failover_count` | Number of failovers |

## CREATE REPLICA_GROUP

```sql
CREATE REPLICA_GROUP standard_ha
    REPLICAS 3
    CONSISTENCY QUORUM;

CREATE REPLICA_GROUP sync_ha
    REPLICAS 3
    CONSISTENCY SYNCHRONOUS
    TYPE STRATEGY NODE_AWARE
    PLACEMENT NODE_AWARE;

CREATE REPLICA_GROUP async_two
    REPLICAS 2
    CONSISTENCY ASYNCHRONOUS
    PLACEMENT RACK_AWARE;
```

### Clause reference

| Clause | Values | Description |
|--------|--------|-------------|
| `REPLICAS n` | `1`, `3`, `5`, … | Target copy count |
| `CONSISTENCY` | `QUORUM`, `SYNCHRONOUS`, `ASYNCHRONOUS` | Write acknowledgment policy |
| `TYPE` / `STRATEGY` | `PRIMARY_REPLICA`, `MULTI_PRIMARY`, `OBSERVER` | Replication topology |
| `PLACEMENT` | `NODE_AWARE`, `RACK_AWARE`, `REGION_AWARE` | Where replicas are placed |

## Bind to a table

```sql
CREATE REPLICA_GROUP finance_ha REPLICAS 3 CONSISTENCY QUORUM;

CREATE TABLE sales (
    id     Int64,
    amount Float64
) ENGINE = Memory
  REPLICA_GROUP finance_ha;
```

Combined with sharding and storage:

```sql
CREATE TABLE orders (
    id          Int64,
    customer_id Int64,
    total       Float64
) ENGINE = Memory
  STORAGE_UNIT su_local
  SHARD_GROUP customer_shards
  REPLICA_GROUP standard_ha;
```

## ALTER REPLICA_GROUP

```sql
ALTER REPLICA_GROUP standard_ha SET REPLICAS 5;
ALTER REPLICA_GROUP standard_ha SET CONSISTENCY SYNCHRONOUS;
```

## SHOW

```sql
SHOW REPLICA_GROUPS;
SHOW REPLICATION STATUS;
```

`SHOW REPLICATION STATUS` reports per-replica state, lag, and group health.

## DESCRIBE / DROP

```sql
DESCRIBE REPLICA_GROUP standard_ha;
DROP REPLICA_GROUP standard_ha;
DROP REPLICA GROUP IF EXISTS old_group;
```

## Python example

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
client.query("CREATE DATABASE IF NOT EXISTS repl_test")
client.query("USE repl_test")

client.query(
    "CREATE REPLICA_GROUP standard_ha REPLICAS 3 CONSISTENCY QUORUM"
)
client.query(
    "CREATE TABLE sales (id Int64, amount Float64) ENGINE=Memory "
    "REPLICA_GROUP standard_ha"
)

r = client.query("SHOW REPLICA_GROUPS")
assert "standard_ha" in r.body.lower()

r = client.query("DESCRIBE REPLICA_GROUP standard_ha")
assert "quorum" in r.body.lower() or "3" in r.body

r = client.query("SHOW REPLICATION STATUS")
assert r.ok()
```

## Operational patterns

### Finance-grade synchronous replication

```sql
CREATE REPLICA_GROUP finance_sync
    REPLICAS 3
    CONSISTENCY SYNCHRONOUS
    PLACEMENT NODE_AWARE;
```

### Read-scale observer replicas

```sql
CREATE REPLICA_GROUP analytics_read
    REPLICAS 3
    CONSISTENCY ASYNCHRONOUS
    TYPE OBSERVER;
```

### Failover monitoring

After node loss, check:

```sql
SHOW REPLICATION STATUS;
SHOW NODE REPLICAS worker_01;
```

## Related

- [nodes-and-clusters.md](nodes-and-clusters.md) — replica placement targets nodes
- [sharding.md](sharding.md) — combine with `SHARD_GROUP`
- `scripts/test_replica_groups_api.py`, `test_replica_groups_failover_api.py`
