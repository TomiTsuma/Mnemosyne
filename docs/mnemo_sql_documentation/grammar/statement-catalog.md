# Statement Catalog

Every top-level statement accepted by `Parser::parse_query()`, with minimal examples. Statements not listed here will cause a syntax error.

## SELECT

```sql
SELECT expr [, ...] [FROM ...] [WHERE ...] [GROUP BY ...] [HAVING ...] [ORDER BY ...] [LIMIT ...];
```

## INSERT

```sql
INSERT INTO t [(cols)] VALUES (v1, v2), (v3, v4);
```

## CREATE

| Form | Example |
|------|---------|
| Database | `CREATE DATABASE [IF NOT EXISTS] db` |
| Table | `CREATE TABLE [IF NOT EXISTS] t (c1 Type, ...) [ENGINE = e] [STORAGE_UNIT su] [SHARD_GROUP sg] [REPLICA_GROUP rg]` |
| View | `CREATE VIEW [IF NOT EXISTS] v AS SELECT ...` |
| Materialized view | `CREATE MATERIALIZED VIEW [IF NOT EXISTS] mv AS SELECT ...` |
| Storage unit | `CREATE STORAGE_UNIT su TYPE LOCAL PATH '...'` |
| Node | `CREATE NODE n TYPE COMPUTE ROLE WORKER` |
| Cluster | `CREATE CLUSTER c` |
| Replica group | `CREATE REPLICA_GROUP rg REPLICAS 3 CONSISTENCY QUORUM` |
| Shard group | `CREATE SHARD_GROUP sg TYPE HASH KEY id SHARDS 16` |
| Connector | `CREATE CONNECTOR c TYPE REST BASE_URL '...'` |
| Pipeline | `CREATE PIPELINE p [OWNER 'team']` |
| Stage | `CREATE STAGE s IN PIPELINE p [ORDER n]` |
| Task | `CREATE TASK t IN STAGE s IN PIPELINE p TYPE SQL BODY '...' [DEPENDS ON t2]` |
| Trigger | `CREATE TRIGGER tr ON PIPELINE p SCHEDULE 'cron'` |
| Stream | `CREATE STREAM s [TOPIC t] [RETAIN n DAYS \| RETAIN FOREVER]` |
| Topic | `CREATE TOPIC t [PARTITIONS n] [RETAIN ...]` |
| Consumer group | `CREATE CONSUMER_GROUP g` |
| Model | `CREATE MODEL m TYPE CLASSIFICATION [...]` |
| Model template | `CREATE MODEL_TEMPLATE t TYPE ...` |
| Feature set | `CREATE FEATURE_SET fs FROM t ENTITY_KEY(k) FEATURES(a,b) TARGET y` |
| Dataset | `CREATE DATASET d FROM t [MODEL m]` |
| Training job | `CREATE TRAINING_JOB j MODEL m FEATURE_SET fs FRAMEWORK ... ALGORITHM ...` |
| Tuning job | `CREATE TUNING_JOB j ... SEARCH_SPACE(...) TRIALS n` |

## DROP / DETACH / TRUNCATE

See [ddl/drop-and-truncate.md](../ddl/drop-and-truncate.md).

## ALTER

| Form | Example |
|------|---------|
| Table columns | `ALTER TABLE t ADD COLUMN c Type` |
| Node | `ALTER NODE n SET role value` |
| Replica group | `ALTER REPLICA_GROUP rg SET REPLICAS 5` |
| Shard group | `ALTER SHARD_GROUP sg SET SHARDS 32` |
| Connector | `ALTER CONNECTOR c SET key value` |
| Stream | `ALTER STREAM s SET RETENTION 30 DAYS` |
| Pipeline | `ALTER PIPELINE p SET OWNER 'x'` |

## USE

```sql
USE [DATABASE] db_name;
```

## SHOW

Bare `SHOW` → databases. Otherwise see [metadata/show-and-describe.md](../metadata/show-and-describe.md).

## DESCRIBE / DESC

```sql
DESCRIBE [kind] name;
```

## EXPLAIN

```sql
EXPLAIN <any supported statement>;
```

## REFRESH

```sql
REFRESH MATERIALIZED VIEW mv_name;
```

## REGISTER / DRAIN / REMOVE NODE

```sql
REGISTER NODE n HOST 'host' PORT 9000;
DRAIN NODE n;
REMOVE NODE [IF EXISTS] n;
```

## TEST / DISCOVER

```sql
TEST CONNECTOR c;
DISCOVER SCHEMA FROM CONNECTOR c;
```

## RUN / PAUSE / RESUME

```sql
RUN PIPELINE p;
RUN TRAINING_JOB j;
RUN TUNING_JOB j;
PAUSE PIPELINE p;
RESUME PIPELINE p;
```

## PUBLISH / SUBSCRIBE

```sql
PUBLISH topic_name [VALUES (...)];
SUBSCRIBE stream_name [CONSUMER_GROUP g] [LIMIT n];
```

## DEPLOY / PREDICT / EVALUATE / COMPARE / GENERATE

```sql
DEPLOY [MODEL] m:v AS endpoint;
PREDICT [MODEL] m:v FOR (k=v, ...);
PREDICT [MODEL] m:v WITH (f=v, ...);
PREDICT [MODEL] m:v FROM table;
EVALUATE [MODEL] m:v;
COMPARE MODELS m1:v1, m2:v2;
GENERATE [USING] [MODEL] m PROMPT 'text';
```

## Unsupported statements

Attempting these produces **Parser: unexpected token**:

| Statement | Status |
|-----------|--------|
| `UPDATE ... SET ...` | Not implemented |
| `DELETE FROM ...` | Not implemented |
| `DROP DATABASE ...` | Not implemented |
| `SET setting = value` | Not implemented |

## Version reference

When extending this catalog, update:

1. `src/Parsers/parser.cpp` — `parse_query()` dispatch
2. `src/Parsers/ast.h` — `QueryAST` fields
3. This file and [full-grammar.md](full-grammar.md)
