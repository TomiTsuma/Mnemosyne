# Queries (DQL)

Mnemo SQL supports read queries through `SELECT`. This section covers analytical query patterns.

## Basic SELECT

```sql
SELECT * FROM customers;

SELECT name, age FROM customers;

SELECT name AS customer_name, age FROM customers;
```

The select list may contain expressions, column references, and aggregate function calls. Aliases use `AS alias` or are parsed inline after the expression in some forms.

## SELECT without FROM

If `FROM` is omitted, the select list is evaluated in a single-row context (when supported by the interpreter).

## WHERE

Filter rows before grouping:

```sql
SELECT * FROM orders
WHERE amount > 100 AND status = 'completed';
```

`WHERE` accepts boolean expressions combined with `AND` / `OR`. Comparison operators: `=`, `!=`, `<>`, `<`, `>`, `<=`, `>=`.

## GROUP BY and HAVING

```sql
SELECT region, sum(amount) AS total, count(*) AS n
FROM orders
GROUP BY region
HAVING total > 1000;
```

Aggregate functions in the select list must align with `GROUP BY` columns (standard analytical SQL rules enforced by the analyzer).

## ORDER BY

```sql
SELECT region, sum(amount) AS total
FROM orders
GROUP BY region
ORDER BY total DESC, region ASC;
```

`ASC` is default; `DESC` sorts descending. Order keys may be column names or expressions.

## LIMIT

```sql
SELECT * FROM orders ORDER BY id LIMIT 10;
```

Mnemo uses **offset, count** comma form when two integers are provided:

```sql
-- LIMIT offset, count  (skip offset rows, return count rows)
SELECT * FROM orders ORDER BY id LIMIT 20, 10;
```

Single-argument form: `LIMIT n` returns the first `n` rows (offset 0).

## DISTINCT, UNION, WITH

These constructs appear in the grammar stub (`grammar.y`) but are **not** fully wired in the current recursive-descent `parse_select`. Do not rely on them until parser support is added.

## EXPLAIN

See [metadata/explain.md](../metadata/explain.md):

```sql
EXPLAIN SELECT * FROM orders WHERE amount > 10;
```

## Further reading

- [select-reference.md](select-reference.md) — select list and FROM details
- [joins.md](joins.md) — inner/left/right joins
- [aggregation.md](aggregation.md) — aggregate and window functions
- [connector-queries.md](connector-queries.md) — `FROM CONNECTOR`
