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

### ALTER CLUSTER {#alter-cluster}

Changes which nodes belong to a cluster. Steps are separated by commas and applied
all-or-nothing: if any step fails, no node changes.

```sql
ALTER CLUSTER production ADD NODE worker_01;
ALTER CLUSTER production ADD NODE worker_02, ADD NODE worker_03;
ALTER CLUSTER production REMOVE NODE worker_03;
```

A node belongs to one cluster. `ADD NODE` of a node that already belongs to another cluster
fails; `REMOVE NODE` it from that cluster first. Adding a node that is already a member does
nothing, and `REMOVE NODE` of a non-member fails. See
[entities/nodes-and-clusters.md](../entities/nodes-and-clusters.md).

### ALTER STORAGE_UNIT {#alter-storage-unit}

```sql
ALTER STORAGE_UNIT warehouse SET RETENTION 365 DAYS;
ALTER STORAGE_UNIT warehouse SET RETENTION FOREVER;
```

`RETENTION` is the only property. The policy is recorded and shown by `SHOW STORAGE_UNITS` and
`DESCRIBE STORAGE_UNIT`, but **nothing expires data yet**: `DESCRIBE` reports
`retention_enforced` as `false`. The count must be a whole number of at least 1 and is followed
by `DAY` or `DAYS`. See [entities/storage-units.md](../entities/storage-units.md).

### ALTER VIEW {#alter-view}

Replaces a view's query.

```sql
ALTER VIEW active_customers AS
SELECT * FROM customers WHERE age < 30 AND deleted = 0;
```

The change is refused, and the old definition kept, if the name is not a view, a table or view
it reads does not exist, the new query would read the view itself (directly or through other
views), or the new query, or any view that reads this one, would no longer run. Because the
definitions are checked by running them, `ALTER VIEW` reads the views' source tables once.
Materialized views built on an altered view are not checked; a break shows up on their next
refresh. See [ddl/views.md](views.md).

### ALTER MATERIALIZED VIEW {#alter-materialized-view}

Sets how a materialized view is refreshed.

```sql
ALTER MATERIALIZED VIEW age_counts SET REFRESH EVERY 1 HOUR;
ALTER MATERIALIZED VIEW age_counts SET REFRESH EVERY 30 SECONDS;
ALTER MATERIALIZED VIEW age_counts SET REFRESH MANUAL;
```

Units are `SECOND(S)`, `MINUTE(S)`, `HOUR(S)` and `DAY(S)`; the interval is at least 1. The
statement changes the policy only and does not refresh the data. Write `MATERIALIZED VIEW` as
two words, as everywhere else in the grammar.

## Grammar pattern

```ebnf
alter_table ::= ALTER TABLE table_name alter_command ( ',' alter_command )*
alter_command ::= ADD [ COLUMN ] name type
                | DROP [ COLUMN ] name
                | MODIFY [ COLUMN ] name type

alter_entity ::= ALTER entity_kind name SET property value ( ',' SET ... )*

alter_cluster ::= ALTER CLUSTER name cluster_step ( ',' cluster_step )*
cluster_step ::= ( ADD | REMOVE ) NODE node_name

alter_storage_unit ::= ALTER STORAGE_UNIT name SET RETENTION ( integer ( DAY | DAYS ) | FOREVER )

alter_view ::= ALTER VIEW name AS select

alter_materialized_view ::= ALTER MATERIALIZED VIEW name SET REFRESH
                            ( EVERY integer unit | MANUAL )
unit ::= SECOND | SECONDS | MINUTE | MINUTES | HOUR | HOURS | DAY | DAYS
```

See entity docs for valid properties per object type.
