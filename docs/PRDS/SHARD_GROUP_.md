# Product Requirements Document (PRD)

# SHARD_GROUP First-Class Entity

**Product:** Mnemosyne Data Platform
**Component:** Distributed Data Distribution Layer
**Version:** 1.0
**Status:** Phase 1 implemented

---

## Implementation status (Phase 1)

**Implemented:** HASH strategy, static round-robin shard placement on cluster nodes,
catalog CRUD, SQL surface, table attachment with reference counting, `SHOW SHARD_GROUPS` /
`SHOW SHARDS` / `SHOW SHARD_STATUS`, node `partition_ids` integration, Python E2E tests.

**Deferred:** `REBALANCE`, RANGE/LIST/COMPOSITE routing, hash-based INSERT/SELECT fan-out,
planner `EXCHANGE` wiring, stream/index attachment, physical data movement.

---

# 1. Overview

## Purpose

The SHARD_GROUP entity defines how Mnemosyne distributes data across nodes within a cluster.

A shard group specifies:

* Partitioning strategy
* Shard count
* Shard ownership
* Data distribution rules
* Rebalancing policies

Shard groups provide the foundation for:

* Horizontal scalability
* Parallel query execution
* Distributed storage
* Distributed streaming
* Workload balancing

---

# 2. Vision

Mnemosyne should allow partitioning strategies to be defined once and reused across multiple objects.

Instead of:

```sql
CREATE TABLE customers
HASH(customer_id)
PARTITIONS 16;
```

Mnemosyne uses:

```sql
CREATE SHARD_GROUP customer_distribution
TYPE HASH
KEY customer_id
SHARDS 16;
```

Then:

```sql
CREATE TABLE customers
SHARD_GROUP customer_distribution;
```

This creates centralized management of data distribution policies.

---

# 3. Goals

## Functional Goals

* Define reusable partitioning policies.
* Support multiple sharding strategies.
* Support automatic shard assignment.
* Enable distributed query execution.
* Enable distributed storage.
* Enable automatic rebalancing.

## Non-Functional Goals

* Scalability
* Performance
* Even workload distribution
* Fault tolerance
* Operational simplicity

---

# 4. Core Concepts

A shard group defines how logical data is partitioned.

Example:

```sql
CREATE SHARD_GROUP customer_distribution
TYPE HASH
KEY customer_id
SHARDS 16;
```

Result:

```text
Customers
 ├── Shard 0
 ├── Shard 1
 ├── Shard 2
 ├── ...
 └── Shard 15
```

Each shard may be assigned to a different node.

---

# 5. Objects Supporting Shard Groups

Shard groups may be attached to:

## Tables

```sql
CREATE TABLE customers
SHARD_GROUP customer_distribution;
```

---

## Streams

```sql
CREATE STREAM user_events
SHARD_GROUP event_distribution;
```

---

## Indexes

```sql
CREATE INDEX customer_idx
SHARD_GROUP customer_distribution;
```

---

## Document Collections

Future support.

```sql
CREATE DOCUMENT_COLLECTION tickets
SHARD_GROUP ticket_distribution;
```

---

## Graphs

Future support.

```sql
CREATE GRAPH customer_network
SHARD_GROUP graph_distribution;
```

---

# 6. Shard Group Metadata

Each shard group stores:

```text
shard_group_id
name
type
shard_count
shard_key
rebalance_policy
status
created_at
updated_at
owner
```

---

# 7. Sharding Strategies

## HASH

Data distributed using hash functions.

Example:

```sql
TYPE HASH
KEY customer_id;
```

Routing:

```text
hash(customer_id) % shard_count
```

Benefits:

* Even distribution
* Predictable performance

Recommended MVP strategy.

---

## RANGE

Data distributed by value ranges.

Example:

```sql
TYPE RANGE
KEY transaction_date;
```

Distribution:

```text
2024 → Shard 0
2025 → Shard 1
2026 → Shard 2
```

Benefits:

* Efficient range queries
* Time-series workloads

Important for analytics.

---

## LIST

Data distributed using predefined categories.

Example:

```sql
TYPE LIST
KEY country;
```

Distribution:

```text
Kenya → Shard 0
Uganda → Shard 1
Tanzania → Shard 2
```

Useful for regional deployments.

---

## COMPOSITE

Future capability.

Example:

```sql
country
+
customer_id
```

Supports advanced distribution patterns.

---

# 8. Shard Definition

Each shard maintains:

```text
shard_id
node_owner
state
row_count
size_bytes
created_at
```

Example:

```text
Shard 12
Owner: worker_03
Rows: 50M
Size: 120 GB
```

---

# 9. Shard Ownership

Every shard has an owner node.

Example:

```text
Shard 0 → worker_01
Shard 1 → worker_02
Shard 2 → worker_03
```

Responsibilities:

* Writes
* Reads
* Maintenance

---

# 10. Shard States

Supported states:

```text
CREATING
ACTIVE
REBALANCING
SPLITTING
MERGING
DEGRADED
OFFLINE
```

---

# 11. Query Routing

The query planner uses shard metadata to route requests.

Example:

```sql
SELECT *
FROM customers
WHERE customer_id = 123;
```

Planner computes:

```text
hash(123)
```

and routes directly to the correct shard.

Benefits:

* Reduced network traffic
* Lower latency

---

# 12. Distributed Query Execution

Queries spanning multiple shards execute in parallel.

Example:

```sql
SELECT SUM(revenue)
FROM sales;
```

Execution:

```text
Coordinator
 ↓
All Relevant Shards
 ↓
Partial Results
 ↓
Aggregation
 ↓
Client
```

---

# 13. Rebalancing

Mnemosyne automatically redistributes shards.

Triggers:

* New nodes
* Removed nodes
* Capacity imbalance
* Performance imbalance

Example:

```text
Node A Full
 ↓
Move Shards
 ↓
Node B
```

---

# 14. Shard Splitting

Large shards may be split.

Example:

```text
Shard 5
```

becomes:

```text
Shard 5A
Shard 5B
```

Benefits:

* Better distribution
* Improved parallelism

---

# 15. Shard Merging

Small shards may be merged.

Example:

```text
Shard 10
Shard 11
```

becomes:

```text
Shard 10
```

Benefits:

* Reduced overhead
* Simplified management

---

# 16. Streams and Sharding

Streams use shard groups for partition assignment.

Example:

```text
user_events
 ├── Shard 0
 ├── Shard 1
 └── Shard 2
```

Routing:

```text
hash(user_id)
```

determines destination shard.

---

# 17. Replica Group Integration

Shard groups define:

```text
Where data lives
```

Replica groups define:

```text
How many copies exist
```

Example:

```sql
CREATE TABLE sales
SHARD_GROUP sales_distribution
REPLICA_GROUP standard_ha;
```

Result:

```text
16 Shards
3 Replicas Per Shard
```

---

# 18. Monitoring

Mnemosyne tracks:

```text
Shard Size
Shard Growth
Node Distribution
Hot Shards
Query Throughput
```

Commands:

```sql
SHOW SHARD_GROUPS;
```

```sql
SHOW SHARDS;
```

```sql
SHOW SHARD_STATUS;
```

---

# 19. Hot Shard Detection

Mnemosyne identifies uneven load.

Example:

```text
Shard 7
80% Cluster Traffic
```

Actions:

* Alerting
* Splitting
* Rebalancing

---

# 20. Security

Permissions:

```text
CREATE_SHARD_GROUP
ALTER_SHARD_GROUP
DROP_SHARD_GROUP
VIEW_SHARD_GROUP
```

---

# 21. SQL Interface

## Create

```sql
CREATE SHARD_GROUP customer_distribution
TYPE HASH
KEY customer_id
SHARDS 16;
```

---

## Show

```sql
SHOW SHARD_GROUPS;
```

---

## Describe

```sql
DESCRIBE SHARD_GROUP customer_distribution;
```

---

## Alter

```sql
ALTER SHARD_GROUP customer_distribution
SET SHARDS 32;
```

---

## Rebalance

```sql
REBALANCE SHARD_GROUP customer_distribution;
```

---

## Drop

```sql
DROP SHARD_GROUP customer_distribution;
```

---

# 22. Example Policies

## Small Deployment

```sql
CREATE SHARD_GROUP dev_distribution
TYPE HASH
KEY id
SHARDS 4;
```

---

## Medium Production

```sql
CREATE SHARD_GROUP standard_distribution
TYPE HASH
KEY customer_id
SHARDS 32;
```

---

## Time-Series Analytics

```sql
CREATE SHARD_GROUP analytics_distribution
TYPE RANGE
KEY event_timestamp
SHARDS 64;
```

---

## Regional Deployment

```sql
CREATE SHARD_GROUP regional_distribution
TYPE LIST
KEY country;
```

---

# 23. Future Roadmap

Phase 1

* Hash sharding
* Static shard assignment

Phase 2

* Range sharding
* Rebalancing

Phase 3

* Automatic splitting
* Automatic merging

Phase 4

* Multi-region shard placement

Phase 5

* Adaptive sharding
* AI-driven shard optimization

---

# 24. Success Metrics

* Query latency
* Distribution balance
* Rebalancing efficiency
* Hot shard detection accuracy
* Storage utilization
* Throughput scalability

---

# 25. Vision

The SHARD_GROUP entity provides a reusable, policy-driven framework for distributing data across Mnemosyne clusters. By separating distribution policies from individual storage objects, Mnemosyne enables scalable, fault-tolerant, and high-performance distributed storage and processing.

Shard groups become the foundational horizontal scalability abstraction for tables, streams, indexes, documents, graphs, and future Mnemosyne workloads.
