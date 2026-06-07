# Data Layer Entities

The data layer comprises traditional catalog objects: **databases**, **tables**, **views**, and **materialized views**. These are the foundation for analytical SQL, feature engineering, and ML source data.

## DATABASE

A logical namespace for tables, views, and session-scoped catalog objects.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Unique database identifier |
| `path` | string | On-disk data directory (engine-dependent) |
| `engine` | string | Default storage backend for the database |
| `tables` | list | Attached table names |

### SQL

```sql
CREATE DATABASE IF NOT EXISTS analytics;
CREATE DATABASE churn_ml;

USE analytics;
USE DATABASE churn_ml;   -- DATABASE keyword is optional

SHOW DATABASES;
```

### Python client example

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")

for sql in [
    "CREATE DATABASE IF NOT EXISTS analytics",
    "USE analytics",
    "SHOW DATABASES",
]:
    r = client.query(sql)
    assert r.ok(), r.body
```

### Notes

- `DROP DATABASE` is not parsed today; drop tables individually.
- Each HTTP session tracks a **current database**; unqualified names resolve there.
- Model-layer objects (`FEATURE_SET`, `MODEL`, etc.) are also scoped to the current database.

---

## TABLE

Structured columnar data with an optional storage engine and placement bindings.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Table name within the current database |
| `columns` | map | Column name → data type (`Int64`, `Float64`, `String`, `DateTime`, …) |
| `engine` | string | Storage engine (`Memory`, `File`, `MergeTree`, …) |
| `storage_unit` | string | Optional `STORAGE_UNIT` binding |
| `shard_group` | string | Optional `SHARD_GROUP` binding |
| `replica_group` | string | Optional `REPLICA_GROUP` binding |

### CREATE TABLE

```sql
CREATE TABLE orders (
    id       Int64,
    region   String,
    amount   Float64,
    status   String,
    created  DateTime
) ENGINE = Memory;

CREATE TABLE IF NOT EXISTS customers (
    customer_id Int64,
    name        String,
    tenure      Int64,
    monthly_charges Float64
) ENGINE = Memory
  STORAGE_UNIT su_local
  SHARD_GROUP customer_distribution
  REPLICA_GROUP standard_ha;
```

### Load data

```sql
INSERT INTO orders VALUES
    (1, 'US-East', 99.50, 'paid', '2025-06-01 09:00:00'),
    (2, 'US-West', 42.00, 'pending', '2025-06-01 10:15:00'),
    (3, 'EU', 120.00, 'paid', '2025-06-02 08:30:00');
```

### Query and introspect

```sql
SELECT region, sum(amount) AS revenue
FROM orders
WHERE status = 'paid'
GROUP BY region
ORDER BY revenue DESC;

SHOW TABLES;
DESCRIBE orders;
DESC TABLE customers;
```

### Feature-engineering table (ML prep)

Derive training-ready columns before defining a `FEATURE_SET`:

```sql
CREATE TABLE ml_customers AS
SELECT
    customer_id,
    tenure,
    monthly_charges,
    support_calls,
    churn
FROM raw_customers
WHERE tenure IS NOT NULL;
```

Or build incrementally:

```sql
CREATE TABLE feature_candidates (
    customer_id Int64,
    tenure Int64,
    monthly_charges Float64,
    support_calls Int64,
    avg_call_duration Float64,
    churn Int64
) ENGINE = Memory;

INSERT INTO feature_candidates
SELECT
    c.customer_id,
    c.tenure,
    c.monthly_charges,
    count(s.call_id) AS support_calls,
    avg(s.duration_sec) AS avg_call_duration,
    c.churn
FROM customers c
LEFT JOIN support_calls s ON s.customer_id = c.customer_id
GROUP BY c.customer_id, c.tenure, c.monthly_charges, c.churn;
```

### Placement clauses

Placement clauses appear after `ENGINE` and are order-independent:

```sql
CREATE TABLE t (id Int64, data String)
ENGINE = File
STORAGE_UNIT su_local
SHARD_GROUP hash_by_id
REPLICA_GROUP ha_three;
```

See [storage-units.md](storage-units.md), [sharding.md](sharding.md), [replication.md](replication.md).

---

## VIEW

A named `SELECT` stored in the catalog. Views are virtual — they do not persist rows.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | View name |
| `query` | string | Underlying `SELECT` definition |
| `columns` | list | Output column names (inferred from query) |

### SQL

```sql
CREATE VIEW active_customers AS
SELECT customer_id, name, tenure
FROM customers
WHERE status = 'active';

CREATE VIEW IF NOT EXISTS high_value AS
SELECT *
FROM customers
WHERE monthly_charges > 80;

SELECT * FROM active_customers WHERE tenure > 12;
SELECT name FROM high_value ORDER BY monthly_charges DESC LIMIT 20;

SHOW VIEWS;
DESCRIBE VIEW active_customers;
DROP VIEW IF EXISTS high_value;
```

### Use in ML feature discovery (read-only)

Views help document business logic before freezing a `FEATURE_SET`:

```sql
CREATE VIEW churn_training_view AS
SELECT
    customer_id,
    tenure,
    monthly_charges,
    support_calls,
    churn
FROM customers
WHERE churn IS NOT NULL;

-- Later, bind the feature set to the underlying table or a materialized snapshot
CREATE FEATURE_SET churn_features
    FROM customers
    ENTITY_KEY(customer_id)
    FEATURES(tenure, monthly_charges, support_calls)
    TARGET churn;
```

---

## MATERIALIZED VIEW

A persisted query result. Refresh explicitly or via pipeline tasks.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Materialized view name |
| `query` | string | Source `SELECT` |
| `row_count` | uint64 | Rows last materialized |
| `last_refreshed_at` | timestamp | Last successful refresh |

### SQL

```sql
CREATE MATERIALIZED VIEW revenue_by_region AS
SELECT region, sum(amount) AS total, count(*) AS order_count
FROM orders
GROUP BY region;

REFRESH MATERIALIZED VIEW revenue_by_region;

SELECT * FROM revenue_by_region ORDER BY total DESC;

SHOW MATERIALIZED VIEWS;
DESCRIBE MATERIALIZED VIEW revenue_by_region;
DROP MATERIALIZED VIEW IF EXISTS revenue_by_region;
```

### Chart-ready aggregates

Materialized views are ideal for dashboard datasets:

```sql
CREATE MATERIALIZED VIEW daily_churn_rate AS
SELECT
    toDate(created) AS day,
    sum(churn) AS churned,
    count(*) AS total,
    sum(churn) / count(*) AS churn_rate
FROM ml_customers
GROUP BY day;

REFRESH MATERIALIZED VIEW daily_churn_rate;

SELECT day, churn_rate FROM daily_churn_rate ORDER BY day;
```

### Pipeline-driven refresh

```sql
CREATE TASK refresh_dashboard IN STAGE transform IN PIPELINE nightly_etl
    TYPE SQL
    BODY 'REFRESH MATERIALIZED VIEW daily_churn_rate';
```

See [pipelines.md](pipelines.md) and [../ddl/views.md](../ddl/views.md).
