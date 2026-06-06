# Databases

## CREATE DATABASE

```sql
CREATE DATABASE analytics;
CREATE DATABASE IF NOT EXISTS analytics;
```

Creates a new database namespace for tables, views, and session context.

## USE database

Switch the session default database:

```sql
USE analytics;
USE DATABASE analytics;   -- optional DATABASE keyword
```

See [metadata/use-database.md](../metadata/use-database.md).

## SHOW DATABASES

```sql
SHOW DATABASES;
```

Lists all databases. Bare `SHOW` (without a following object keyword) also lists databases.

## DROP DATABASE

**Not implemented** in the current parser. Remove objects individually or via future DDL.

## Naming

Database names follow identifier rules. Use `USE` before unqualified `CREATE TABLE` in integration tests:

```sql
CREATE DATABASE IF NOT EXISTS pipeline_test_db;
USE pipeline_test_db;
CREATE TABLE raw_events (id Int64, payload String);
```
