# Schema and DDL

Data Definition Language commands create and modify catalog objects: databases, tables, views, and Mnemo first-class entities.

## Statement summary

| Statement | Purpose |
|-----------|---------|
| `CREATE DATABASE` | New database namespace |
| `CREATE TABLE` | Columnar table with engine |
| `CREATE VIEW` | Named query |
| `CREATE MATERIALIZED VIEW` | Stored query result |
| `CREATE` (entities) | Storage units, nodes, connectors, … |
| `DROP` / `DETACH` | Remove catalog objects |
| `TRUNCATE TABLE` | Clear table data |
| `ALTER TABLE` | Add/drop/modify columns |
| `ALTER` (entities) | `SET` properties on nodes, streams, … |
| `REFRESH MATERIALIZED VIEW` | Recompute materialized view |

## IF NOT EXISTS / IF EXISTS

```sql
CREATE DATABASE IF NOT EXISTS analytics;
CREATE TABLE IF NOT EXISTS t (id Int64);
DROP VIEW IF EXISTS old_view;
```

## Subsections

- [databases.md](databases.md)
- [tables.md](tables.md)
- [views.md](views.md)
- [drop-and-truncate.md](drop-and-truncate.md)
- [alter.md](alter.md)

Entity-specific DDL lives under [entities/](../entities/).
