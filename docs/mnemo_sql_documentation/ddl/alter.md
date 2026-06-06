# ALTER

## ALTER TABLE

Column-level changes on tables in the current database:

```sql
ALTER TABLE customers ADD COLUMN email String;

ALTER TABLE customers ADD email String;   -- COLUMN keyword optional

ALTER TABLE customers DROP COLUMN email;
ALTER TABLE customers DROP email;

ALTER TABLE customers MODIFY COLUMN age Int32;
ALTER TABLE customers MODIFY age Int32;
```

Multiple commands in one statement (comma-separated):

```sql
ALTER TABLE t
    ADD COLUMN a Int64,
    ADD COLUMN b String;
```

## ALTER entity SET

Infrastructure objects use `ALTER … SET property = value` (or property value pairs).

### ALTER NODE

```sql
ALTER NODE worker_01 SET ROLE WORKER;
```

### ALTER REPLICA_GROUP

```sql
ALTER REPLICA_GROUP standard_ha SET REPLICAS 5;
ALTER REPLICA_GROUP standard_ha SET CONSISTENCY SYNCHRONOUS;
```

### ALTER SHARD_GROUP

```sql
ALTER SHARD_GROUP customer_distribution SET SHARDS 32;
```

### ALTER CONNECTOR

```sql
ALTER CONNECTOR api_rest SET HOST '10.0.0.5';
```

Property names are connector-specific (`HOST`, `PORT`, `PATH`, custom keys).

### ALTER STREAM

```sql
ALTER STREAM events SET RETENTION 30 DAYS;
ALTER STREAM events SET RETENTION FOREVER;
```

### ALTER PIPELINE

```sql
ALTER PIPELINE etl SET OWNER 'data-team';
```

## Grammar pattern

```ebnf
alter_table ::= ALTER TABLE table_name alter_command ( ',' alter_command )*
alter_command ::= ADD [ COLUMN ] name type
                | DROP [ COLUMN ] name
                | MODIFY [ COLUMN ] name type

alter_entity ::= ALTER entity_kind name SET property value ( ',' SET ... )*
```

See entity docs for valid properties per object type.
