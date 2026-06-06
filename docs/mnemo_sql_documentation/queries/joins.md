# Joins

Mnemo SQL supports explicit joins with `ON` conditions.

## Inner join

```sql
SELECT c.name, o.total
FROM customers c
INNER JOIN orders o ON c.customer_id = o.customer_id;
```

`INNER JOIN` and `JOIN` are equivalent (default join type is `INNER`).

## Left outer join

```sql
SELECT c.name, o.total
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id;

-- explicit OUTER keyword optional
FROM customers c LEFT OUTER JOIN orders o ON ...
```

## Right outer join

```sql
FROM orders o
RIGHT JOIN customers c ON o.customer_id = c.customer_id;
```

## Multiple joins

Joins chain left-to-right:

```sql
SELECT *
FROM a
JOIN b ON a.id = b.a_id
LEFT JOIN c ON b.id = c.b_id;
```

## Join conditions

`ON` requires a boolean expression, typically equality on keys:

```sql
ON t1.id = t2.foreign_id
ON t1.id = t2.foreign_id AND t2.active = TRUE
```

Cross joins (cartesian product) are not exposed as a dedicated keyword; omitting `ON` is a syntax error for explicit joins.

## Comma-separated FROM (legacy)

The grammar stub allows comma-separated table references in `FROM`. The current parser focuses on explicit `JOIN` syntax after the primary table. Prefer explicit joins for clarity.

## Connector joins

Join local tables with connector resources by materializing or using subqueries:

```sql
SELECT l.*, r.name
FROM local_ids l
JOIN (
    SELECT id, name FROM CONNECTOR api.customers
) r ON l.id = r.id;
```

(Subquery in FROM depends on full subquery-as-table support in the interpreter.)
