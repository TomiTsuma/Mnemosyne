# Syntax Conventions

## Case sensitivity

- **Keywords** are case-insensitive: `select`, `SELECT`, and `SeLeCt` are equivalent.
- **Identifiers** (table and column names) preserve the case used at creation time; unquoted identifiers are stored as parsed.

## Identifiers

Unquoted identifiers match `[A-Za-z_][A-Za-z0-9_]*`.

Qualified names use dot notation:

```sql
SELECT t.column FROM my_table AS t;
SELECT * FROM CONNECTOR my_rest.customers;
```

## String and numeric literals

| Form | Example |
|------|---------|
| Integer | `42`, `-7` |
| Float | `3.14`, `.5` |
| String | `'hello'`, `'it''s fine'` (single-quoted; escape `'` by doubling) |
| Boolean | `TRUE`, `FALSE` |
| Null | `NULL` |

## Comments

```sql
-- line comment to end of line

/* block comment
   spanning lines */
```

The lexer also accepts `//` line comments (extension).

## Statement terminators

Semicolons are optional for single statements. Multiple statements in one batch depend on server behavior.

## Property values

Entity DDL (connectors, storage units, pipelines) accepts property values as:

- Single-quoted strings: `PATH '/data/mnemo'`
- Bare identifiers or numbers: `PORT 5432`, `TYPE REST`
- Unquoted words parsed as identifiers: `OWNER 'analytics'`

## `IF NOT EXISTS` / `IF EXISTS`

Supported on most `CREATE` and `DROP` statements:

```sql
CREATE TABLE IF NOT EXISTS t (id Int64);
DROP TABLE IF EXISTS t;
DROP CONNECTOR IF EXISTS old_api;
REMOVE NODE IF EXISTS worker_01;
```

## Naming first-class entities

Multi-word entity kinds use underscores in SQL:

| Entity | Keyword form |
|--------|----------------|
| Storage unit | `STORAGE_UNIT` or `STORAGE UNIT` |
| Replica group | `REPLICA_GROUP` or `REPLICA GROUP` |
| Shard group | `SHARD_GROUP` or `SHARD GROUP` |
| Consumer group | `CONSUMER_GROUP` |
| Materialized view | `MATERIALIZED VIEW` |
| Training job | `TRAINING_JOB` |
| Feature set | `FEATURE_SET` |

Both underscored and spaced forms are accepted where the parser defines keyword aliases.

## Error handling

Syntax errors return a message identifying the unexpected token. Execution errors (unknown table, type mismatch) are returned separately from parse success. Integration tests in `scripts/` demonstrate expected success and failure patterns.
