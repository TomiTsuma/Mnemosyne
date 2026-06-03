## Level 1: Database and Table Operations

These verify metadata management and basic DDL.

```sql
CREATE DATABASE sales_db;

USE sales_db;

SHOW DATABASES;

SHOW TABLES;
```

Create a table:

```sql
CREATE TABLE customers (
    customer_id INT,
    name STRING,
    age INT
);
```

Verify schema:

```sql
DESCRIBE customers;
```

Drop table:

```sql
DROP TABLE customers;
```

---

## Level 2: Insert and Read Operations

Create test table:

```sql
CREATE TABLE customers (
    customer_id INT,
    name STRING,
    age INT
);
```

Insert data:

```sql
INSERT INTO customers VALUES
(1, 'Alice', 25),
(2, 'Bob', 30),
(3, 'Charlie', 35);
```

Basic retrieval:

```sql
SELECT * FROM customers;
```

Projection:

```sql
SELECT name FROM customers;
```

Projection of multiple columns:

```sql
SELECT customer_id, age
FROM customers;
```

---

## Level 3: Filtering

Single predicate:

```sql
SELECT *
FROM customers
WHERE age > 30;
```

Equality:

```sql
SELECT *
FROM customers
WHERE customer_id = 2;
```

Multiple predicates:

```sql
SELECT *
FROM customers
WHERE age > 25
  AND age < 40;
```

OR condition:

```sql
SELECT *
FROM customers
WHERE age < 25
   OR age > 30;
```

---

## Level 4: Sorting

Ascending:

```sql
SELECT *
FROM customers
ORDER BY age;
```

Descending:

```sql
SELECT *
FROM customers
ORDER BY age DESC;
```

Multiple sort keys:

```sql
SELECT *
FROM customers
ORDER BY age DESC, name ASC;
```

---

## Level 5: Aggregations

Create sales table:

```sql
CREATE TABLE sales (
    sale_id INT,
    customer_id INT,
    amount DOUBLE
);
```

```sql
INSERT INTO sales VALUES
(1,1,100),
(2,1,200),
(3,2,150),
(4,3,300);
```

Count:

```sql
SELECT COUNT(*)
FROM sales;
```

Sum:

```sql
SELECT SUM(amount)
FROM sales;
```

Average:

```sql
SELECT AVG(amount)
FROM sales;
```

Minimum:

```sql
SELECT MIN(amount)
FROM sales;
```

Maximum:

```sql
SELECT MAX(amount)
FROM sales;
```

Multiple aggregates:

```sql
SELECT
    COUNT(*),
    SUM(amount),
    AVG(amount),
    MIN(amount),
    MAX(amount)
FROM sales;
```

---

## Level 6: GROUP BY

Revenue per customer:

```sql
SELECT
    customer_id,
    SUM(amount)
FROM sales
GROUP BY customer_id;
```

Count sales per customer:

```sql
SELECT
    customer_id,
    COUNT(*)
FROM sales
GROUP BY customer_id;
```

Multiple aggregates:

```sql
SELECT
    customer_id,
    COUNT(*),
    SUM(amount),
    AVG(amount)
FROM sales
GROUP BY customer_id;
```

---

## Level 7: HAVING

```sql
SELECT
    customer_id,
    SUM(amount) AS revenue
FROM sales
GROUP BY customer_id
HAVING revenue > 200;
```

or

```sql
SELECT
    customer_id,
    SUM(amount)
FROM sales
GROUP BY customer_id
HAVING SUM(amount) > 200;
```

---

## Level 8: Joins

Create product table:

```sql
CREATE TABLE products (
    product_id INT,
    product_name STRING
);
```

Orders table:

```sql
CREATE TABLE orders (
    order_id INT,
    customer_id INT,
    product_id INT,
    amount DOUBLE
);
```

Inner join:

```sql
SELECT
    c.name,
    o.amount
FROM customers c
JOIN orders o
ON c.customer_id = o.customer_id;
```

Multiple joins:

```sql
SELECT
    c.name,
    p.product_name,
    o.amount
FROM orders o
JOIN customers c
    ON o.customer_id = c.customer_id
JOIN products p
    ON o.product_id = p.product_id;
```

Left join:

```sql
SELECT
    c.name,
    o.amount
FROM customers c
LEFT JOIN orders o
ON c.customer_id = o.customer_id;
```

---

## Level 9: Subqueries

Scalar subquery:

```sql
SELECT *
FROM sales
WHERE amount >
(
    SELECT AVG(amount)
    FROM sales
);
```

IN subquery:

```sql
SELECT *
FROM customers
WHERE customer_id IN
(
    SELECT customer_id
    FROM sales
);
```

---

## Level 10: Analytical Workloads

Monthly revenue:

```sql
SELECT
    sale_month,
    SUM(revenue)
FROM fact_sales
GROUP BY sale_month
ORDER BY sale_month;
```

Top customers:

```sql
SELECT
    customer_id,
    SUM(amount) AS revenue
FROM sales
GROUP BY customer_id
ORDER BY revenue DESC
LIMIT 10;
```

Star-schema query:

```sql
SELECT
    d.year,
    p.category,
    SUM(f.revenue)
FROM fact_sales f
JOIN dim_date d
    ON f.date_key = d.date_key
JOIN dim_product p
    ON f.product_key = p.product_key
GROUP BY
    d.year,
    p.category;
```

---

## Level 11: Window Functions (If Supported)

Running total:

```sql
SELECT
    customer_id,
    amount,
    SUM(amount) OVER (
        PARTITION BY customer_id
        ORDER BY sale_id
    )
FROM sales;
```

Ranking:

```sql
SELECT
    customer_id,
    SUM(amount) AS revenue,
    RANK() OVER (
        ORDER BY SUM(amount) DESC
    )
FROM sales
GROUP BY customer_id;
```

---

## Level 12: Stress Tests

Large scan:

```sql
SELECT COUNT(*)
FROM fact_sales;
```

Large aggregation:

```sql
SELECT
    customer_id,
    SUM(amount)
FROM fact_sales
GROUP BY customer_id;
```

Large sort:

```sql
SELECT *
FROM fact_sales
ORDER BY amount DESC
LIMIT 100;
```

Join + Group By:

```sql
SELECT
    c.region,
    SUM(f.revenue)
FROM fact_sales f
JOIN customers c
ON f.customer_id = c.customer_id
GROUP BY c.region;
```

---

## Level 13: Columnar Database Specific Tests

These are particularly important for an analytical DBMS:

Column pruning:

```sql
SELECT customer_id
FROM fact_sales;
```

Predicate pushdown:

```sql
SELECT *
FROM fact_sales
WHERE sale_date = '2026-01-01';
```

Late materialization:

```sql
SELECT customer_id
FROM fact_sales
WHERE revenue > 1000;
```

Aggregation-heavy OLAP query:

```sql
SELECT
    country,
    category,
    YEAR(order_date),
    SUM(revenue),
    COUNT(*),
    AVG(revenue)
FROM fact_sales
GROUP BY
    country,
    category,
    YEAR(order_date);
```

---

A practical milestone is:

1. DDL (`CREATE`, `DROP`, `SHOW`)
2. DML (`INSERT`, `SELECT`)
3. Filtering (`WHERE`)
4. Sorting (`ORDER BY`)
5. Aggregates (`COUNT`, `SUM`, `AVG`)
6. `GROUP BY`
7. `HAVING`
8. Joins
9. Subqueries
10. Window functions
11. Large-scale analytical queries
12. Performance tests on millions of rows

