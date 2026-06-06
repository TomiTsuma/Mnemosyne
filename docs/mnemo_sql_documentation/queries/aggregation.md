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

Aggregate names are case-insensitive (`SUM`, `sum`, `Sum`).

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

Group keys may be column names or expressions:

```sql
SELECT toYear(ts) AS y, count(*) FROM events GROUP BY toYear(ts);
```

(`toYear` and similar helpers require registration in the function factory.)

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

Window function **evaluation** depends on processor support; syntax is parsed into `ASTFunction::WindowSpec`.

## Interaction with SELECT list aliases

Prefer repeating expressions or using subqueries when the analyzer does not resolve alias references in `HAVING`/`ORDER BY`.

## Scalar vs aggregate functions

- **Aggregates** — `sum`, `count`, `avg`, `min`, `max` (uppercase keywords also tokenized)
- **Scalars** — lowercase builtins: `add`, `eq`, etc. (see [expressions/functions.md](../expressions/functions.md))

Mixing aggregates and scalars follows standard SQL rules: non-grouped columns must be inside aggregate functions.
