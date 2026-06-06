# Mnemo SQL Documentation

Mnemo SQL is the query and control language for **Mnemosyne** (Mnemo): a column-oriented analytical database with extensions for storage, distributed execution, connectors, pipelines, streaming, and machine learning. This documentation is written for developers who integrate with Mnemo via HTTP/TCP, client libraries, or embedded interpreters.

## What Mnemo SQL covers

Mnemo SQL is not a generic ANSI SQL clone. It combines:

- **Analytical SQL** — `SELECT`, `INSERT`, aggregations, joins, window functions (partial)
- **Catalog DDL** — databases, tables, views, materialized views
- **First-class entities** — storage units, nodes, replication, sharding, connectors, pipelines, streams
- **Model layer** — feature sets, training jobs, deployment, prediction, evaluation

Statements are parsed by a recursive-descent parser (`src/Parsers/parser.cpp`). The canonical grammar sketch lives in `src/Parsers/grammar.y` and the complete reference in [grammar/full-grammar.md](grammar/full-grammar.md).

## Documentation map

| Section | Path | Topics |
|---------|------|--------|
| Getting started | [getting-started/](getting-started/) | Running queries, conventions, session context |
| Data types | [data-types/](data-types/) | Column types, literals, compatibility |
| Queries (DQL) | [queries/](queries/) | `SELECT`, joins, `GROUP BY`, `ORDER BY`, `LIMIT` |
| Data manipulation | [dml/](dml/) | `INSERT` |
| Schema & DDL | [ddl/](ddl/) | `CREATE` / `DROP` / `ALTER` for tables and catalog objects |
| Metadata | [metadata/](metadata/) | `SHOW`, `DESCRIBE`, `EXPLAIN`, `USE` |
| Expressions | [expressions/](expressions/) | Operators, functions, precedence |
| First-class entities | [entities/](entities/) | Storage, nodes, replication, sharding, connectors, pipelines, streams |
| Model layer | [model-layer/](model-layer/) | ML catalog, training, inference |
| Grammar reference | [grammar/](grammar/) | Full EBNF, lexer, keywords, statement catalog |

## Quick example

```sql
CREATE DATABASE IF NOT EXISTS analytics;
USE analytics;

CREATE TABLE orders (
    id     Int64,
    region String,
    amount Float64
) ENGINE = Memory;

INSERT INTO orders VALUES
    (1, 'US-East', 99.50),
    (2, 'US-West', 42.00);

SELECT region, sum(amount) AS total
FROM orders
GROUP BY region
ORDER BY total DESC
LIMIT 10;
```

Send the query to the server (default HTTP port `1143`):

```bash
curl -s "http://127.0.0.1:1143/?query=SELECT%201" 
# or POST JSON: {"query": "SELECT 1", "format": "JSON"}
```

## Statement overview

Top-level statements recognized by the parser:

| Category | Statements |
|----------|------------|
| Query | `SELECT` |
| DML | `INSERT` |
| DDL | `CREATE`, `DROP`, `TRUNCATE`, `DETACH`, `ALTER`, `REFRESH` |
| Session | `USE` |
| Introspection | `SHOW`, `DESCRIBE`, `EXPLAIN` |
| Infrastructure | `REGISTER`, `DRAIN`, `REMOVE`, `TEST`, `DISCOVER` |
| Pipelines | `RUN`, `PAUSE`, `RESUME` (pipelines); `RUN` (training/tuning jobs) |
| Streaming | `PUBLISH`, `SUBSCRIBE` |
| Model layer | `DEPLOY`, `PREDICT`, `EVALUATE`, `COMPARE`, `GENERATE` |

## Not yet implemented (parser)

The following appear in older docs or common SQL dialects but are **not** parsed today:

- `UPDATE`, `DELETE`
- `DROP DATABASE`
- `SET` session variables (keyword reserved in lexer)
- `WITH` (CTE), `UNION`, `DISTINCT` select modifier (grammar stub only)
- `BETWEEN`, `CASE`, `CAST`, `LIKE`, `IS NULL` in expressions (grammar stub; verify before use)

When in doubt, consult [grammar/statement-catalog.md](grammar/statement-catalog.md) and the parser source.

## Related resources

- Integration tests with runnable SQL: `scripts/test_*_api.py`
- Legacy grammar summary: `docs/GRAMMAR.md`
- Architecture: `docs/ARCHITECTURE.md`
