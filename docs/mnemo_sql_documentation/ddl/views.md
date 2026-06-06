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

## SHOW

```sql
SHOW VIEWS;
SHOW MATERIALIZED VIEWS;
```

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
