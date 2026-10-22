# Views

## CREATE VIEW

A view is a named `SELECT` stored in the catalog.

```sql
CREATE VIEW active_customers AS
SELECT * FROM customers WHERE age < 30;

CREATE VIEW IF NOT EXISTS young_names AS
SELECT customer_id, name FROM customers WHERE age < 25;
```

Query views like tables:

```sql
SELECT * FROM active_customers;
SELECT name FROM young_names WHERE customer_id = 1;
```

## CREATE MATERIALIZED VIEW

Materialized views persist query results (implementation refreshes on `REFRESH` or pipeline tasks).

```sql
CREATE MATERIALIZED VIEW age_counts AS
SELECT age, count(*) AS n
FROM customers
GROUP BY age;
```

### Refresh

```sql
REFRESH MATERIALIZED VIEW age_counts;
```

### Scheduled refresh {#scheduled-refresh}

A materialized view is `MANUAL` by default: it is populated when created and changes only on
`REFRESH`. To refresh it automatically:

```sql
ALTER MATERIALIZED VIEW age_counts SET REFRESH EVERY 1 HOUR;
ALTER MATERIALIZED VIEW age_counts SET REFRESH MANUAL;
```

A background scheduler checks once a second and refreshes every scheduled view, in any
database, once its interval has passed since the last refresh attempt (scheduled or manual). A
scheduled refresh does not change the database a client has selected with `USE`.

If a refresh fails, for example because a source table was dropped, the view keeps its last
good data, its `status` becomes `DEGRADED` with the error in `last_error`, and the scheduler
tries again one interval later. A later successful refresh makes it `ACTIVE` again.

## SHOW

```sql
SHOW VIEWS;
SHOW MATERIALIZED VIEWS;
```

`SHOW MATERIALIZED VIEWS` returns, after `name`: `status`, `refresh` (`MANUAL` or
`SCHEDULED`), `refresh_interval_seconds` (`0` for `MANUAL`), `last_refreshed_at` (UTC, of the
last successful refresh) and `last_error`.

## ALTER {#alter}

```sql
ALTER VIEW active_customers AS
SELECT * FROM customers WHERE age < 30 AND deleted = 0;
```

`ALTER VIEW` replaces the view's query. It is refused, and the old query kept, if the new one
would read the view itself or would stop the view, or any view that reads it, from running.
For the refresh policy of a materialized view, see [Scheduled refresh](#scheduled-refresh).
Details are in [ddl/alter.md](alter.md#alter-view).

## DROP

```sql
DROP VIEW active_customers;
DROP VIEW IF EXISTS young_names;
DROP MATERIALIZED VIEW age_counts;
DROP MATERIALIZED VIEW IF EXISTS age_counts;
```

## DESCRIBE

```sql
DESCRIBE VIEW active_customers;
DESCRIBE MATERIALIZED VIEW age_counts;
```

## Pipelines and views

Pipelines often create views and materialized views in SQL tasks:

```sql
CREATE TASK build_mv IN STAGE transform IN PIPELINE etl
TYPE SQL BODY 'CREATE MATERIALIZED VIEW IF NOT EXISTS summary AS SELECT ...';
```

See [entities/pipelines.md](../entities/pipelines.md).
