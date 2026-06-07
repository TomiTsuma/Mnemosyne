# Sharding (Shard Groups)

`SHARD_GROUP` defines how table rows are partitioned across shards by a key column. Use with `REPLICA_GROUP` for distributed, highly available tables.

## Purpose

- Horizontal partitioning for scale-out
- Hash, range, list, or composite routing strategies
- Rebalance shards without changing table DDL

## Properties

### ShardGroupEntry

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Group identifier |
| `strategy` | enum | `HASH`, `RANGE`, `LIST`, `COMPOSITE` |
| `shard_count` | uint32 | Number of shards (`SHARDS n`) |
| `shard_key` | string | Column used for routing (`KEY col`) |
| `status` | enum | `ONLINE`, `DEGRADED`, `OFFLINE` |
| `reference_count` | size_t | Tables using this group |
| `owner` | string | Optional owner |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

### ShardMember (runtime)

| Property | Description |
|----------|-------------|
| `shard_id` | Shard identifier |
| `node_id` | Node hosting the shard |
| `state` | `CREATING`, `ACTIVE`, `REBALANCING`, `SPLITTING`, … |
| `row_count` | Rows in shard |
| `size_bytes` | Storage size |

## CREATE SHARD_GROUP

```sql
CREATE SHARD_GROUP customer_distribution
    TYPE HASH
    KEY customer_id
    SHARDS 16;

CREATE SHARD_GROUP order_range
    TYPE RANGE
    KEY order_date
    SHARDS 8;

CREATE SHARD_GROUP region_list
    TYPE LIST
    KEY region
    SHARDS 4;
```

### Clause reference

| Clause | Description |
|--------|-------------|
| `TYPE` | `HASH`, `RANGE`, `LIST`, `COMPOSITE` |
| `KEY` | Column name for shard routing |
| `SHARDS n` | Target shard count |

## Bind to a table

```sql
CREATE TABLE customers (
    customer_id Int64,
    name        String
) ENGINE = Memory
  SHARD_GROUP customer_distribution;
```

Full placement stack:

```sql
CREATE TABLE sales (
    id          Int64,
    customer_id Int64,
    amount      Float64
) ENGINE = Memory
  STORAGE_UNIT su_local
  SHARD_GROUP customer_distribution
  REPLICA_GROUP standard_ha;
```

## ALTER SHARD_GROUP

```sql
ALTER SHARD_GROUP customer_distribution SET SHARDS 32;
```

Increasing shard count triggers rebalancing (`REBALANCING` state on members).

## SHOW

```sql
SHOW SHARD GROUPS;
SHOW SHARDS;
SHOW SHARD STATUS;
```

`SHOW SHARD STATUS` reports per-shard node assignment, row counts, and state.

## DESCRIBE / DROP

```sql
DESCRIBE SHARD_GROUP customer_distribution;
DROP SHARD_GROUP customer_distribution;
DROP SHARD GROUP IF EXISTS old_shards;
```

## Query considerations

Mnemo routes queries to relevant shards based on predicates on the shard key:

```sql
-- Likely single-shard (point lookup on shard key)
SELECT * FROM customers WHERE customer_id = 42;

-- Cross-shard aggregate
SELECT region, count(*) FROM customers GROUP BY region;
```

## Python example

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
client.query("CREATE DATABASE IF NOT EXISTS shard_test")
client.query("USE shard_test")

client.query(
    "CREATE SHARD_GROUP customer_distribution "
    "TYPE HASH KEY customer_id SHARDS 16"
)
client.query(
    "CREATE TABLE customers (customer_id Int64, name String) "
    "ENGINE=Memory SHARD_GROUP customer_distribution"
)

r = client.query("SHOW SHARD GROUPS")
assert "customer_distribution" in r.body.lower()

r = client.query("DESCRIBE SHARD_GROUP customer_distribution")
assert "hash" in r.body.lower() or "16" in r.body

client.query("INSERT INTO customers VALUES (1, 'Alice'), (2, 'Bob')")
r = client.query("SELECT count(*) FROM customers")
assert r.ok()
```

## Operational patterns

### Scale-out re-sharding

```sql
-- Double shards during low-traffic window
ALTER SHARD_GROUP customer_distribution SET SHARDS 32;
SHOW SHARD STATUS;  -- monitor REBALANCING → ACTIVE
```

### Co-locate with replicas

```sql
CREATE SHARD_GROUP events_by_day TYPE HASH KEY event_id SHARDS 32;
CREATE REPLICA_GROUP events_ha REPLICAS 3 CONSISTENCY QUORUM;

CREATE TABLE events (...)
ENGINE = Memory
SHARD_GROUP events_by_day
REPLICA_GROUP events_ha;
```

## Related

- [replication.md](replication.md)
- [nodes-and-clusters.md](nodes-and-clusters.md)
- `scripts/test_shard_groups_api.py`
