# SELECT Reference

## Syntax

```ebnf
select_statement ::= SELECT select_list
                     [ FROM from_clause ]
                     [ WHERE boolean_expr ]
                     [ GROUP BY expr_list ]
                     [ HAVING boolean_expr ]
                     [ ORDER BY order_list ]
                     [ LIMIT limit_clause ]

select_list      ::= select_item ( ',' select_item )*
select_item      ::= expr [ [ AS ] identifier ]
                   | '*'

order_list       ::= order_key [ ASC | DESC ] ( ',' order_key [ ASC | DESC ] )*
order_key        ::= identifier | identifier '.' identifier

limit_clause     ::= count
                   | count OFFSET offset
                   | offset ',' count

from_clause      ::= table_source [ join_clause ... ]

table_source     ::= qualified_table [ alias ]
                   | CONNECTOR connector_name '.' resource_name

join_clause      ::= ( INNER | LEFT [ OUTER ] | RIGHT [ OUTER ] )? JOIN table_ref ON boolean_expr
```

## Select list

### Star expansion

```sql
SELECT * FROM customers;
```

### Expressions and aliases

```sql
SELECT
    customer_id,
    upper(name) AS name_upper,   -- if upper() registered
    age + 1 AS age_next
FROM customers;
```

An alias applies to the whole select item, so `age > 25 AS older` names the comparison, and `SUM(x) / COUNT(*) AS avg_x` names the ratio. `AS` is optional (`name n`). After `AS` any word is accepted, including a word that is also a keyword (`AS bucket`).

Without an alias, a column keeps its name and any other expression is named by its text, for example `COUNT(*)` or `(spend * 2)`.

### Aggregates in select list

When using aggregates without `GROUP BY`, the entire result is one group, and it produces exactly one row even when the input is empty. With `GROUP BY`, every non-aggregated column must appear in the group key; otherwise the statement fails with `Column '...' must appear in GROUP BY or inside an aggregate function`. `SELECT *` cannot be combined with aggregation.

## Aliases in other clauses {#aliases-in-other-clauses}

A select alias can be used in `WHERE`, `GROUP BY`, `HAVING` and `ORDER BY`. A real column of the same name takes precedence over the alias.

```sql
SELECT age > 25 AS older, COUNT(*) AS n
FROM customers
GROUP BY older
HAVING n > 3
ORDER BY n DESC;
```

## ORDER BY {#order-by}

Keys are column names, select aliases, or qualified columns (`o.amount`). Several keys may be combined, each with its own `ASC` (default) or `DESC`. Ordering is stable. Values sort in this order: `NULL`, numbers (compared numerically), then strings (compared bytewise).

A key can name a column that is not in the select list, except in aggregate queries, where only output columns and aliases are available.

`ORDER BY` an expression (`ORDER BY age * 2`, `ORDER BY SUM(x)`) is rejected with a syntax error. Select the expression with an alias and order by the alias instead.

## LIMIT and OFFSET {#limit-and-offset}

```sql
SELECT id FROM customers ORDER BY id LIMIT 3;            -- first 3 rows
SELECT id FROM customers ORDER BY id LIMIT 3 OFFSET 2;   -- skip 2, then 3 rows
SELECT id FROM customers ORDER BY id LIMIT 2, 3;         -- MySQL form: offset 2, count 3
SELECT id FROM customers LIMIT 0;                        -- no rows
```

`LIMIT 0` returns no rows (it does not mean "no limit"), and an offset past the end returns an empty result. Both the count and the offset must be non-negative integers. `LIMIT` is applied after `ORDER BY`.

## Strictness {#strictness}

A statement that uses something unsupported fails instead of returning a partial or empty result:

- Tokens left over after the statement (`... WHERE a > 1 BANANA`) are a syntax error.
- Unknown functions, unknown columns in `ORDER BY`, and `LIKE` raise errors.
- Comparing text with a number never matches (`WHERE id = 'x'` returns no rows).
- Arithmetic on non-numeric values, and modulo by zero, raise errors.

Columns have no null bitmap yet, so `AVG`, `MIN` and `MAX` over zero rows return `0` instead of `NULL`.

## FROM clause

### Tables

```sql
FROM orders
FROM orders o          -- alias: o
FROM analytics.orders  -- database-qualified (when supported)
```

### Connector resources

Query external data through a registered connector:

```sql
SELECT * FROM CONNECTOR api_rest.customers;

SELECT order_id, total
FROM CONNECTOR api_rest.orders
WHERE total > 20;
```

Pattern: `CONNECTOR <connector_name>.<resource_name>`

Resources are discovered via `DISCOVER SCHEMA FROM CONNECTOR` (see [entities/connectors.md](../entities/connectors.md)).

## Table aliases

An alias may follow the table name without `AS`:

```sql
SELECT o.amount FROM orders o;
```

## Subqueries in expressions

Parenthesized `SELECT` in `WHERE` with `IN`:

```sql
SELECT * FROM orders
WHERE customer_id IN (SELECT customer_id FROM vip_customers);
```

The parser builds a subquery expression for `IN ( SELECT … )`.

## Precedence and parentheses

Use parentheses to override default precedence in expressions (see [expressions/operators.md](../expressions/operators.md)).
