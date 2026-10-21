# Aggregation and Window Functions

## Aggregate functions

Built-in aggregates registered in `src/AggregateFunctions/`:

| Function | Description | Example |
|----------|-------------|---------|
| `count(*)` | Row count | `count(*)` |
| `count(expr)` | Non-null count | `count(customer_id)` |
| `sum(expr)` | Sum | `sum(amount)` |
| `avg(expr)` | Average | `avg(amount)` |
| `min(expr)` | Minimum | `min(created_at)` |
| `max(expr)` | Maximum | `max(score)` |
| `quantile(p)(expr)` | Quantile at level `p` in `[0, 1]`, linearly interpolated | `quantile(0.9)(amount)` |
| `median(expr)` | Same as `quantile(0.5)(expr)` | `median(age)` |
| `percentile(expr, p)` | Quantile at percent `p` in `[0, 100]` | `percentile(age, 90)` |

Aggregate names are case-insensitive (`SUM`, `sum`, `Sum`). `quantile(expr, p)` with the level as a second argument is accepted for compatibility. The level must be a numeric literal inside the valid range.

### Result types {#result-types}

- `count` returns an integer.
- `sum` returns an integer when every value is an integer, otherwise a floating-point number.
- `avg`, `quantile`, `median` and `percentile` return floating-point numbers, so `avg(age)` keeps its fraction.
- `min` and `max` return a value of the column's own type and also work on strings.
- `NULL` values are skipped by every aggregate except `count(*)`.
- Non-numeric values passed to `sum`, `avg` or a quantile raise an error.

An aggregate over zero rows produces one row (when there is no `GROUP BY`): `count` is `0`, and the others show `0` because columns have no null bitmap yet.

### Example

```sql
SELECT
    region,
    count(*)        AS orders,
    sum(amount)     AS revenue,
    avg(amount)     AS avg_order,
    min(amount)     AS smallest,
    max(amount)     AS largest
FROM orders
GROUP BY region;
```

## GROUP BY

```sql
SELECT status, count(*) FROM orders GROUP BY status;
SELECT region, status, count(*) FROM orders GROUP BY region, status;
```

Group keys may be column names, select aliases, or expressions, and may be strings, integers or floating-point numbers. Keys compare by value, so `1` and `1.0` form one group while `1.2` and `1.7` do not. Groups are returned in order of first appearance; add `ORDER BY` for a defined order.

```sql
SELECT age > 25 AS older, count(*) AS n FROM customers GROUP BY age > 25;
SELECT age > 25 AS older, count(*) AS n FROM customers GROUP BY older;
```

Aggregates can be combined in an expression, for example `SUM(spend) / COUNT(*) AS avg_spend`. Scalar helper functions such as `toYear` are not available yet; an unknown function raises `Unknown function`.

## HAVING

Filter groups after aggregation:

```sql
SELECT region, sum(amount) AS total
FROM orders
GROUP BY region
HAVING total > 5000;
```

## Window functions (OVER)

The parser accepts window specifications after aggregate or function calls:

```sql
SELECT
    region,
    amount,
    sum(amount) OVER (PARTITION BY region ORDER BY amount DESC) AS running_total
FROM orders;
```

### OVER clause syntax

```ebnf
window_spec ::= OVER '('
                  [ PARTITION BY expr_list ]
                  [ ORDER BY order_list ]
                ')'
```

Example:

```sql
avg(amount) OVER (PARTITION BY region)
rank() OVER (ORDER BY amount DESC)   -- if rank() implemented
```

### Supported window functions {#supported-window-functions}

- `NTILE(n) OVER (...)` splits each partition into `n` buckets numbered from 1, ordered by the window `ORDER BY`. Earlier buckets receive the extra rows when the size does not divide evenly. `n` must be a positive integer literal.
- `count`, `sum`, `avg`, `min`, `max` and the quantile functions with `OVER (...)` are computed over the **whole partition**. Window frames are not supported yet, so `sum(amount) OVER (PARTITION BY region ORDER BY amount)` is the partition total on every row, not a running total.

A window function cannot be combined with `GROUP BY` or a plain aggregate in the same `SELECT`; this raises an error. Name the result with `AS`, otherwise the column is called `<function>_over`.

## Interaction with SELECT list aliases {#select-alias-interaction}

Select aliases can be referenced in `WHERE`, `GROUP BY`, `HAVING` and `ORDER BY` (see [select-reference.md](select-reference.md#aliases-in-other-clauses)). `HAVING` accepts either the alias (`HAVING n > 1`) or the aggregate itself (`HAVING SUM(v) > 5`).

## Scalar vs aggregate functions

- **Aggregates** — `sum`, `count`, `avg`, `min`, `max`, `quantile`, `median`, `percentile` (uppercase keywords also tokenized)
- **Scalars** — lowercase builtins: `add`, `eq`, etc. (see [expressions/functions.md](../expressions/functions.md))

Mixing aggregates and scalars follows standard SQL rules: non-grouped columns must be inside aggregate functions.
