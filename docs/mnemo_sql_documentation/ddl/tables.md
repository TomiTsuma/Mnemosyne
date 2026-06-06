# Tables

## CREATE TABLE

```sql
CREATE TABLE table_name (
    column_name type_name,
    column_name type_name,
    ...
)
[ ENGINE = engine_name ]
[ STORAGE_UNIT unit_name ]
[ SHARD_GROUP group_name ]
[ REPLICA_GROUP group_name ];
```

### Example

```sql
CREATE TABLE orders (
    id       Int64,
    region   String,
    amount   Float64,
    status   String
) ENGINE = Memory;
```

### With placement entities

```sql
CREATE TABLE sharded_customers (
    customer_id Int64,
    name        String
) ENGINE = Memory
  STORAGE_UNIT local_disk
  SHARD_GROUP customer_distribution
  REPLICA_GROUP standard_ha;
```

### IF NOT EXISTS

```sql
CREATE TABLE IF NOT EXISTS orders (...);
```

## Column definitions

```ebnf
column_def ::= identifier type_identifier
```

Optional `NOT NULL`, `DEFAULT`, and `COMMENT` appear in the grammar stub; the minimal parser accepts `name type` pairs.

## Engines

```sql
ENGINE = Memory
ENGINE = MergeTree   -- when storage backend supports it
```

Default in AST: `Memory`.

## SHOW TABLES

```sql
SHOW TABLES;
```

Lists tables in the **current database**.

## DESCRIBE TABLE

```sql
DESCRIBE customers;
DESC customers;
DESCRIBE TABLE customers;   -- TABLE keyword optional in some paths
```

Returns column names and types.

## Related

- [alter.md](alter.md) — column changes
- [drop-and-truncate.md](drop-and-truncate.md)
- [entities/storage-units.md](../entities/storage-units.md)
- [entities/sharding.md](../entities/sharding.md)
