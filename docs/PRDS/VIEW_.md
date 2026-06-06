For Mnemosyne, I would make a distinction between:

1. **VIEW** → logical query abstraction.
2. **MATERIALIZED_VIEW** → physical precomputed dataset.

Many systems blur these together, but for an analytical platform like Mnemosyne, they should be completely separate first-class entities.

A VIEW should behave like a reusable semantic layer that sits above tables, streams, metrics, and even AI objects.

# Product Requirements Document (PRD)

## VIEW First-Class Entity

**Product:** Mnemosyne Data Platform
**Component:** Semantic Data Layer
**Version:** 1.0
**Status:** Draft

---

# 1. Overview

## Purpose

The VIEW entity provides a virtual representation of data without physically storing records.

Views allow users to:

* Simplify complex queries
* Create reusable business logic
* Enforce security boundaries
* Build semantic data models
* Abstract physical storage structures

A view behaves as a logical dataset that can be queried like a table while remaining dynamically computed from underlying sources.

---

# 2. Vision

Views should become the primary mechanism for exposing business-ready data.

Instead of exposing raw tables:

```sql
sales_transactions
customer_events
product_catalog
```

users interact with:

```sql
monthly_revenue
active_customers
customer_health
```

This creates a semantic layer between physical storage and business consumption.

---

# 3. Goals

## Functional Goals

* Support reusable query definitions.
* Abstract underlying storage.
* Support joins and transformations.
* Support security filtering.
* Support metric definitions.
* Support streaming data sources.

## Non-Functional Goals

* Simplicity
* Maintainability
* Query optimization
* Governance

---

# 4. Core Concepts

A view stores query logic rather than data.

Example:

```sql
CREATE VIEW active_customers AS
SELECT *
FROM customers
WHERE status = 'ACTIVE';
```

Internally:

```text
View Definition
      ↓
Stored Metadata
      ↓
Executed At Query Time
```

No records are physically stored.

---

# 5. View Metadata

Each view maintains:

```text
view_id
view_name
database_id
schema_id
owner
definition
status
created_at
updated_at
dependencies
```

---

# 6. View Types

## Standard View

Computed at query execution.

Example:

```sql
CREATE VIEW active_customers AS
SELECT *
FROM customers
WHERE active = true;
```

---

## Secure View

Masks underlying data.

Example:

```sql
CREATE VIEW customer_public AS
SELECT
    customer_id,
    country
FROM customers;
```

Sensitive columns remain hidden.

---

## Parameterized View

Future capability.

Example:

```sql
SELECT *
FROM monthly_sales(2026);
```

---

## Streaming View

Built on streams.

Example:

```sql
CREATE VIEW realtime_orders AS
SELECT *
FROM order_stream;
```

Returns continuously updated results.

---

# 7. Supported Sources

Views may reference:

## Tables

```sql
customers
```

---

## Views

```sql
customer_summary
```

---

## Streams

```sql
user_events
```

---

## Materialized Views

```sql
monthly_revenue
```

---

## Metrics

Future capability.

```sql
revenue
```

---

# 8. Dependency Management

Mnemosyne tracks dependencies.

Example:

```text
sales_dashboard
    ↓
monthly_revenue
    ↓
sales
```

This enables:

* Impact analysis
* Safe schema changes
* Lineage tracking

---

# 9. Query Processing

When a query references a view:

```sql
SELECT *
FROM active_customers;
```

Mnemosyne performs:

```text
View Expansion
      ↓
Query Rewrite
      ↓
Optimization
      ↓
Execution
```

---

# 10. View Chaining

Views may reference other views.

Example:

```text
customer_base
      ↓
active_customers
      ↓
high_value_customers
```

Mnemosyne resolves dependencies automatically.

---

# 11. Security

Views may enforce row-level filtering.

Example:

```sql
CREATE VIEW kenya_sales AS
SELECT *
FROM sales
WHERE country = 'Kenya';
```

Users only see authorized data.

---

# 12. Column-Level Security

Views may hide sensitive columns.

Example:

```sql
CREATE VIEW customer_safe AS
SELECT
    customer_id,
    first_name
FROM customers;
```

Excluded columns:

```text
email
phone
credit_card
```

---

# 13. Semantic Layer Support

Views should become Mnemosyne's business semantic layer.

Example:

Raw Tables:

```text
orders
customers
payments
```

Business View:

```sql
CREATE VIEW customer_health AS
SELECT ...
```

Used by:

* Dashboards
* Reports
* Models
* Agents

---

# 14. Streaming Views

Views may consume stream data.

Example:

```sql
CREATE VIEW active_sessions AS
SELECT
    user_id,
    COUNT(*)
FROM user_events
GROUP BY user_id;
```

Updated continuously.

---

# 15. AI-Aware Views

Future capability.

Views may reference AI outputs.

Example:

```sql
CREATE VIEW churn_risk AS
SELECT
    customer_id,
    churn_score
FROM churn_model_predictions;
```

---

# 16. View Optimization

Mnemosyne optimizer may:

* Push predicates
* Eliminate unused columns
* Rewrite joins
* Simplify nested views

Example:

```text
View
 ↓
Rewrite
 ↓
Optimized Plan
```

---

# 17. View States

Supported states:

```text
CREATING
ACTIVE
INVALID
REFRESHING
DEPRECATED
```

---

## INVALID

Occurs when dependencies are missing.

Example:

```text
Underlying Table Dropped
```

View becomes invalid until repaired.

---

# 18. Lineage Tracking

Mnemosyne automatically records:

```text
Source Tables
Source Views
Columns Used
Transformations
```

Example:

```text
customer_health
 ↓
customers
orders
payments
```

---

# 19. Monitoring

Metrics:

```text
Query Count
Execution Time
Dependency Count
Failure Count
```

Commands:

```sql
SHOW VIEWS;
```

```sql
SHOW VIEW LINEAGE;
```

```sql
DESCRIBE VIEW customer_health;
```

---

# 20. SQL Interface

## Create

```sql
CREATE VIEW active_customers AS
SELECT *
FROM customers
WHERE active = true;
```

---

## Show

```sql
SHOW VIEWS;
```

---

## Describe

```sql
DESCRIBE VIEW active_customers;
```

---

## Alter

```sql
ALTER VIEW active_customers
AS
SELECT *
FROM customers
WHERE active = true
AND deleted = false;
```

---

## Drop

```sql
DROP VIEW active_customers;
```

---

# 21. Future Roadmap

### Phase 1

* Standard views
* Dependency tracking
* Query rewriting

### Phase 2

* Security views
* Lineage tracking

### Phase 3

* Streaming views
* Semantic layer integration

### Phase 4

* Parameterized views
* AI-aware views

### Phase 5

* Self-optimizing views
* Agent-generated views

---

# 22. Success Metrics

* View adoption rate
* Query simplification
* Dashboard reuse
* Query performance
* Lineage coverage
* Security policy compliance

---

# 23. Vision

The VIEW entity serves as the semantic abstraction layer of Mnemosyne, separating business logic from physical storage structures. Views provide reusable, governable, and secure representations of data that power analytics, dashboards, machine learning, streaming applications, and AI-driven decision intelligence.

---

### Mnemosyne-Specific Enhancement

I would actually introduce three related entities:

```sql
CREATE VIEW ...
CREATE MATERIALIZED_VIEW ...
CREATE METRIC ...
```

Responsibilities:

```text
VIEW
  = Business logic

MATERIALIZED_VIEW
  = Performance optimization

METRIC
  = Business KPI definition
```

For example:

```sql
CREATE METRIC monthly_recurring_revenue
AS SUM(subscription_amount);
```

Then:

```sql
CREATE VIEW executive_dashboard AS
SELECT
    monthly_recurring_revenue,
    customer_churn,
    customer_ltv;
```

This is much closer to how modern analytics platforms such as Databricks, Snowflake, Looker, and dbt semantic layers are evolving, and it aligns well with Core&Outline's goal of becoming an intelligence platform rather than just a database.
