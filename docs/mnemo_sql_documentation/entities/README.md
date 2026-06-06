# First-Class Entities

Mnemo extends SQL with catalog objects for storage, cluster topology, data integration, orchestration, and streaming. These are created and managed with SQL DDL and control verbs—not separate REST APIs only.

## Entity map

| Entity | CREATE | DROP | ALTER | SHOW | DESCRIBE |
|--------|--------|------|-------|------|----------|
| Storage unit | ✓ | ✓ | — | ✓ | ✓ |
| Node | ✓ | ✓ | SET | ✓ | ✓ |
| Cluster | ✓ | — | — | ✓ | ✓ |
| Replica group | ✓ | ✓ | SET | ✓ | ✓ |
| Shard group | ✓ | ✓ | SET | ✓ | ✓ |
| Connector | ✓ | ✓ | SET | ✓ | ✓ |
| Pipeline / stage / task / trigger | ✓ | ✓ | SET (pipeline) | ✓ | ✓ (pipeline) |
| Stream / topic / consumer group | ✓ | ✓ | SET (stream) | ✓ | ✓ |
| Model layer | ✓ | ✓ | — | ✓ | ✓ |

## Control verbs (non-DDL)

| Verb | Target |
|------|--------|
| `REGISTER NODE` | Register node host/port |
| `DRAIN NODE` | Graceful drain |
| `REMOVE NODE` | Remove from catalog |
| `TEST CONNECTOR` | Health check |
| `DISCOVER SCHEMA FROM CONNECTOR` | List remote resources |
| `RUN PIPELINE` / `PAUSE` / `RESUME` | Pipeline lifecycle |
| `PUBLISH` / `SUBSCRIBE` | Stream messaging |

## Documentation

- [storage-units.md](storage-units.md)
- [nodes-and-clusters.md](nodes-and-clusters.md)
- [replication.md](replication.md)
- [sharding.md](sharding.md)
- [connectors.md](connectors.md)
- [pipelines.md](pipelines.md)
- [streams.md](streams.md)

Model entities are documented in [model-layer/](../model-layer/).

## Binding entities to tables

```sql
CREATE TABLE t (id Int64, data String)
ENGINE = Memory
STORAGE_UNIT local_files
SHARD_GROUP hash_by_id
REPLICA_GROUP ha_three;
```

Placement clauses are optional and order-independent after column list and `ENGINE`.
