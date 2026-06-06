# Functions

## Scalar functions (registered)

From `src/Functions/function_factory.cpp`:

| Name | Description | Args |
|------|-------------|------|
| `add(a, b)` | Addition | 2 |
| `sub(a, b)` | Subtraction | 2 |
| `mul(a, b)` | Multiplication | 2 |
| `div(a, b)` | Division | 2 |
| `mod(a, b)` | Modulo | 2 |
| `eq(a, b)` | Equal | 2 |
| `ne(a, b)` | Not equal | 2 |
| `gt(a, b)` | Greater than | 2 |
| `lt(a, b)` | Less than | 2 |
| `ge(a, b)` | Greater or equal | 2 |
| `le(a, b)` | Less or equal | 2 |

### Examples

```sql
SELECT add(1, 2), mul(price, 1.1) FROM products;
SELECT * FROM t WHERE eq(status, 'active');
```

Unknown functions fail at analysis/execution with `unknown function`.

## Aggregate functions (registered)

From `src/AggregateFunctions/aggregate_function_factory.cpp`:

| Name | Description |
|------|-------------|
| `count(*)` / `count(expr)` | Count rows or non-null values |
| `sum(expr)` | Sum |
| `avg(expr)` | Average |
| `min(expr)` | Minimum |
| `max(expr)` | Maximum |

### Examples

```sql
SELECT count(*), sum(amount), avg(amount) FROM orders;
SELECT region, max(amount) FROM orders GROUP BY region;
```

Keyword forms `SUM(...)`, `COUNT(*)`, etc. are equivalent at parse time.

## Window functions

Any function name followed by `OVER (...)` is parsed with a window spec:

```sql
sum(amount) OVER (PARTITION BY region ORDER BY order_date)
```

Built-in window implementations depend on the processor layer.

## User-defined functions

Extending Mnemo: register creators in `FunctionFactory::register_function` (C++ plugin path). SQL `CREATE FUNCTION` is not part of the current parser.

## DISTINCT in aggregates

Grammar stub: `count(DISTINCT col)` — verify parser and aggregate support before use.

## Naming conventions

- Scalar builtins use **lowercase** names (`add`, `eq`).
- Aggregates accept **case-insensitive** keywords (`SUM`, `sum`).
