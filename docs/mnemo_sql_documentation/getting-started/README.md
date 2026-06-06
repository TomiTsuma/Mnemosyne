# Getting Started with Mnemo SQL

## Prerequisites

- A running Mnemosyne server (`mnemosyne_server`) — see the project README for build and launch instructions.
- Default HTTP query endpoint: `http://127.0.0.1:1143/`

## Running a query

### HTTP GET

```bash
curl -G "http://127.0.0.1:1143/" --data-urlencode "query=SHOW DATABASES"
```

### HTTP POST (JSON)

```json
{
  "query": "SELECT region, count(*) FROM orders GROUP BY region",
  "format": "JSON"
}
```

Responses return column names, row data, and row counts in JSON (or other formats when supported).

### Python client (tests)

The repository includes a lightweight client used by integration tests:

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
r = client.query("SELECT 1")
assert r.ok()
```

## Typical workflow

1. **Create or select a database** — `CREATE DATABASE …` then `USE db_name`
2. **Define schema** — `CREATE TABLE …` (optionally bind `STORAGE_UNIT`, `SHARD_GROUP`, `REPLICA_GROUP`)
3. **Load data** — `INSERT INTO … VALUES …`
4. **Query** — analytical `SELECT` with filters and aggregates
5. **Operate** — `SHOW`, `DESCRIBE`, entity-specific commands (connectors, pipelines, models)

```sql
CREATE DATABASE IF NOT EXISTS demo;
USE demo;

CREATE TABLE events (id Int64, ts DateTime, payload String) ENGINE Memory;
INSERT INTO events VALUES (1, '2025-01-15 10:00:00', 'click');

SELECT count(*) FROM events;
```

## Sessions and databases

- Each connection has a **current database** (session context).
- `USE analytics` or `USE DATABASE analytics` switches context.
- Unqualified table names resolve in the current database.
- `SHOW DATABASES` lists databases; `SHOW TABLES` lists tables in the current database.

See [metadata/use-database.md](../metadata/use-database.md).

## Syntax conventions

See [syntax-conventions.md](syntax-conventions.md) for identifiers, comments, string literals, and optional semicolons.

## Next steps

- [Queries](../queries/) — analytical SQL
- [DDL](../ddl/) — tables and views
- [Entities](../entities/) — connectors, pipelines, streams, cluster objects
- [Model layer](../model-layer/) — ML workflows
- [Full grammar](../grammar/full-grammar.md) — complete language reference
