# Sharding (Shard Groups)

Shard groups partition table data across shards by key.

## CREATE SHARD GROUP

```sql
CREATE SHARD_GROUP customer_distribution
    TYPE HASH
    KEY customer_id
    SHARDS 16;
```

### Clauses

| Clause | Description |
|--------|-------------|
| `TYPE` | Sharding algorithm (`HASH`, etc.) |
| `KEY` | Column or expression name for shard routing |
| `SHARDS n` | Number of shards |

## Bind to table

```sql
CREATE TABLE customers (customer_id Int64, name String)
ENGINE = Memory
SHARD_GROUP customer_distribution;
```

Combined with replication:

```sql
CREATE TABLE sales (id Int64, customer_id Int64, amount Float64)
ENGINE = Memory
SHARD_GROUP customer_distribution
REPLICA_GROUP standard_ha;
```

## ALTER SHARD GROUP

```sql
ALTER SHARD_GROUP customer_distribution SET SHARDS 32;
```

## SHOW

```sql
SHOW SHARD GROUPS;
SHOW SHARDS;
SHOW SHARD STATUS;
```

## DESCRIBE / DROP

```sql
DESCRIBE SHARD_GROUP customer_distribution;
DROP SHARD_GROUP customer_distribution;
```

See `scripts/test_shard_groups_api.py`.
