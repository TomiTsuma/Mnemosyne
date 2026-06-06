# USE Database

Sets the session's current database for resolving unqualified table and view names.

## Syntax

```sql
USE database_name;
USE DATABASE database_name;
```

Both forms are equivalent (ClickHouse-compatible optional `DATABASE` keyword).

## Example workflow

```sql
CREATE DATABASE IF NOT EXISTS analytics;
USE analytics;

CREATE TABLE events (id Int64) ENGINE Memory;   -- creates analytics.events
SELECT * FROM events;                           -- resolves analytics.events
```

## Interaction with SHOW

After `USE analytics`:

```sql
SHOW TABLES;   -- tables in analytics only
```

Other databases require qualification or switching with `USE`.

## HTTP sessions

Stateless HTTP clients must either:

- Prefix objects with database names where supported, or
- Send `USE db` before subsequent queries on the same session, or
- Include database in connection settings if the server exposes that option

Refer to your client library for session persistence semantics.
