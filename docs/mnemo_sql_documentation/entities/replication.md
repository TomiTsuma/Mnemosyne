# Replication (Replica Groups)

Replica groups define copy count and consistency for highly available data.

## CREATE REPLICA GROUP

```sql
CREATE REPLICA_GROUP standard_ha
    REPLICAS 3
    CONSISTENCY QUORUM;

CREATE REPLICA_GROUP sync_ha
    REPLICAS 3
    CONSISTENCY SYNCHRONOUS
    TYPE STRATEGY NODE_AWARE
    PLACEMENT NODE_AWARE;
```

### Clauses

| Clause | Example values |
|--------|----------------|
| `REPLICAS n` | `3`, `5` |
| `CONSISTENCY` | `QUORUM`, `SYNCHRONOUS`, `ASYNCHRONOUS` |
| `TYPE` / `STRATEGY` | Replica placement strategy |
| `PLACEMENT` | `NODE_AWARE`, etc. |

## Bind to table

```sql
CREATE TABLE sales (id Int64, amount Float64)
ENGINE = Memory
REPLICA_GROUP standard_ha;
```

## ALTER REPLICA GROUP

```sql
ALTER REPLICA_GROUP standard_ha SET REPLICAS 5;
ALTER REPLICA_GROUP standard_ha SET CONSISTENCY SYNCHRONOUS;
```

## SHOW

```sql
SHOW REPLICA_GROUPS;
SHOW REPLICATION STATUS;
```

## DESCRIBE / DROP

```sql
DESCRIBE REPLICA_GROUP standard_ha;
DROP REPLICA_GROUP standard_ha;
DROP REPLICA GROUP IF EXISTS old_group;
```

See `scripts/test_replica_groups_api.py` and `test_replica_groups_failover_api.py`.
