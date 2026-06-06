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
select_item      ::= expr [ AS identifier ]
                   | '*'

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

### Aggregates in select list

When using aggregates without `GROUP BY`, the entire result is one group. With `GROUP BY`, non-aggregated columns must appear in the group key.

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
