Separate first-class entities into **Infrastructure**, **Storage**, **Data**, **Streaming**, **Pipeline**, **AI**, and **Governance** domains.

A good rule is:

> A first-class entity should have its own CREATE, ALTER, DROP, DESCRIBE, SHOW, permissions, metadata, and lifecycle.

If something doesn't need those, it probably shouldn't be first-class.

---

# Infrastructure Layer

These define the physical and distributed platform.

## CLUSTER

Represents an Atlas deployment.

```sql
CREATE CLUSTER production;
```

Examples:

* Production
* Staging
* Development

---

## NODE

Represents a running Atlas instance.

```sql
CREATE NODE worker_01;
```

Stores:

* CPU
* RAM
* Storage
* Status
* Capabilities

---

## STORAGE_UNIT

Physical storage abstraction.

```sql
CREATE STORAGE_UNIT warehouse;
```

Supports:

* Local
* S3
* MinIO
* Ceph
* NFS

---

## REPLICA_GROUP

Replication configuration.

```sql
CREATE REPLICA_GROUP finance_replicas;
```

---

## SHARD_GROUP

Partitioning definition.

```sql
CREATE SHARD_GROUP customer_shards;
```

---

# Organization Layer

Multi-tenancy and ownership.

## ORGANIZATION

Tenant boundary.

```sql
CREATE ORGANIZATION cropnuts;
```

---

## PROJECT

Logical workspace.

```sql
CREATE PROJECT marketing;
```

---

## ENVIRONMENT

Deployment environment.

```sql
CREATE ENVIRONMENT production;
```

Examples:

* Dev
* Test
* Prod

---

# Data Layer

Traditional database objects.

## DATABASE

Logical database.

```sql
CREATE DATABASE analytics;
```

---

## SCHEMA

Namespace.

```sql
CREATE SCHEMA finance;
```

---

## TABLE

Structured persistent data.

```sql
CREATE TABLE customers;
```

Storage engines:

* Row
* Column
* Time-Series
* LSM

---

## VIEW

Virtual query.

```sql
CREATE VIEW active_customers;
```

---

## MATERIALIZED_VIEW

Persisted query result.

```sql
CREATE MATERIALIZED VIEW monthly_revenue;
```

---

## INDEX

Query acceleration.

```sql
CREATE INDEX idx_customer;
```

Types:

* B-tree
* Bitmap
* Inverted
* Vector

---

# Streaming Layer

One of Atlas's differentiators.

## STREAM

Continuous append-only data.

```sql
CREATE STREAM user_events;
```

Examples:

* Clickstream
* IoT
* CDC

---

## TOPIC

Messaging abstraction.

```sql
CREATE TOPIC notifications;
```

Kafka equivalent.

---

## CONSUMER_GROUP

Tracks stream readers.

```sql
CREATE CONSUMER_GROUP dashboard_service;
```

---

## WINDOW

Streaming aggregations.

```sql
CREATE WINDOW last_hour;
```

Examples:

* Tumbling
* Sliding
* Session

---

# Pipeline Layer

Orchestration and processing.

## PIPELINE

Workflow definition.

```sql
CREATE PIPELINE revenue_sync;
```

---

## CONNECTOR

External integration.

```sql
CREATE CONNECTOR stripe;
```

Examples:

* Salesforce
* Shopify
* HubSpot
* Postgres

---

## TASK

Scheduled execution.

```sql
CREATE TASK nightly_refresh;
```

---

## JOB

Pipeline execution instance.

```sql
CREATE JOB refresh_20260606;
```

---

# Search Layer

For Atlas search capabilities.

## INDEX_CATALOG

Search index.

```sql
CREATE SEARCH_INDEX customer_feedback;
```

---

## DOCUMENT_COLLECTION

Unstructured documents.

```sql
CREATE DOCUMENT_COLLECTION support_tickets;
```

---

# Graph Layer

For relationship analytics.

## GRAPH

Graph database object.

```sql
CREATE GRAPH customer_network;
```

---

## NODE_TYPE

Graph node schema.

```sql
CREATE NODE_TYPE customer;
```

---

## EDGE_TYPE

Relationship schema.

```sql
CREATE EDGE_TYPE purchased;
```

---

# AI Layer

This is where Atlas becomes unique.

## FEATURESET

Reusable ML features.

```sql
CREATE FEATURESET customer_health;
```

Examples:

* Lifetime Value
* Purchase Frequency
* Churn Indicators

---

## MODEL

Machine learning model.

```sql
CREATE MODEL churn_predictor;
```

Types:

* Classification
* Regression
* Forecasting
* Ranking
* LLM

---

## MODEL_ENDPOINT

Serving endpoint.

```sql
CREATE MODEL_ENDPOINT churn_api;
```

---

## VECTOR_INDEX

Embedding storage.

```sql
CREATE VECTOR_INDEX customer_embeddings;
```

---

## KNOWLEDGE_BASE

Structured AI knowledge.

```sql
CREATE KNOWLEDGE_BASE finance_knowledge;
```

---

# Decision Intelligence Layer

This is where Atlas could differentiate from most databases.

## METRIC

Business metric definition.

```sql
CREATE METRIC mrr;
```

Examples:

* Revenue
* Churn
* CAC
* LTV

---

## KPI

Monitored business KPI.

```sql
CREATE KPI customer_retention;
```

---

## ALERT

Monitoring rule.

```sql
CREATE ALERT churn_spike;
```

---

## INSIGHT

Generated analytical finding.

```sql
CREATE INSIGHT revenue_decline;
```

---

## RECOMMENDATION

Suggested action.

```sql
CREATE RECOMMENDATION pricing_adjustment;
```

---

## DECISION

Recorded business action.

```sql
CREATE DECISION reduce_prices;
```

This becomes Atlas's organizational memory.

---

# Governance Layer

Enterprise requirements.

## USER

Platform user.

```sql
CREATE USER thomas;
```

---

## ROLE

Access control role.

```sql
CREATE ROLE analyst;
```

---

## POLICY

Security policy.

```sql
CREATE POLICY pii_masking;
```

---

## SECRET

Credential management.

```sql
CREATE SECRET stripe_api_key;
```

---

## AUDIT_LOG

Compliance tracking.

```sql
CREATE AUDIT_LOG security_events;
```

---

# Monitoring Layer

Operational visibility.

## DASHBOARD

Monitoring dashboard.

```sql
CREATE DASHBOARD executive_summary;
```

---

## MONITOR

System monitor.

```sql
CREATE MONITOR cluster_health;
```

---

