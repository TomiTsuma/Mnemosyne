# Mnemosyne Streaming Layer PRD

## Product Requirements Document (PRD)

**Product:** Mnemosyne Data Platform
**Component:** Streaming Layer
**Version:** 1.0
**Status:** Phase 1 Implemented (catalog + in-memory events)

---

# 1. Overview

## Purpose

The Mnemosyne Streaming Layer provides real-time data ingestion, transport, processing, storage, and event-driven execution capabilities across the Mnemosyne platform.

The Streaming Layer serves as the foundation for:

* Event-driven architectures
* Change Data Capture (CDC)
* Real-time analytics
* Monitoring systems
* AI inference pipelines
* Recommendation systems
* Dynamic pricing engines
* Customer behavior tracking
* Data synchronization

Unlike traditional message brokers, the Mnemosyne Streaming Layer is deeply integrated with:

* Tables
* Views
* Materialized Views
* Pipelines
* Models
* Metrics
* Storage Units
* Clusters

Streams become first-class citizens within Mnemosyne.

---

# 2. Vision

Mnemosyne should unify:

```text
Streaming
+
Storage
+
Analytics
+
Machine Learning
+
Decision Intelligence
```

within a single platform.

Users should be able to:

```sql
CREATE STREAM user_events;
```

and immediately:

* Query it
* Persist it
* Transform it
* Train models on it
* Create dashboards from it
* Trigger workflows from it

without moving data into another system.

---

# 3. Goals

## Functional Goals

* Real-time ingestion
* Event persistence
* Event replay
* Distributed streaming
* Stream processing
* Windowed aggregation
* Event routing
* Stream-to-table integration

## Non-Functional Goals

* Horizontal scalability
* Low latency
* High throughput
* Fault tolerance
* Exactly-once processing support
* Multi-tenant isolation

---

# 4. Streaming Architecture

```text
Producer
    ↓
Topic
    ↓
Stream
    ↓
Partitions
    ↓
Consumer Groups
    ↓
Pipelines
    ↓
Tables / Views / Models
```

---

# 5. First-Class Streaming Entities

The Mnemosyne Streaming Layer introduces:

```sql
CREATE TOPIC
CREATE STREAM
CREATE CONSUMER_GROUP
CREATE WINDOW
CREATE STREAM_PROCESSOR
CREATE EVENT_TRIGGER
```

---

# 6. STREAM

## Purpose

A STREAM represents an append-only sequence of ordered events.

Streams are the primary abstraction for real-time data.

Example:

```sql
CREATE STREAM user_events;
```

Examples:

* Website clicks
* Transactions
* Sensor readings
* Application logs
* CDC records

---

### Metadata

```text
stream_id
stream_name
topic_id
storage_unit
shard_group
replica_group
created_at
status
```

---

### Features

* Event persistence
* Replay support
* Partitioning
* Replication
* Ordering guarantees

---

### Example

```sql
CREATE STREAM orders;
```

Insert:

```sql
INSERT INTO orders
VALUES (...);
```

Subscribe:

```sql
SUBSCRIBE orders;
```

---

# 7. TOPIC

## Purpose

A TOPIC is the transport and routing abstraction.

Topics receive events from producers and distribute them to streams and consumers.

Example:

```sql
CREATE TOPIC user_activity;
```

---

### Metadata

```text
topic_id
topic_name
partition_count
retention_policy
replication_policy
```

---

### Responsibilities

* Event ingestion
* Event routing
* Partition assignment
* Retention management

---

### Example

```sql
CREATE TOPIC transactions;
```

Producers publish:

```sql
PUBLISH transactions;
```

---

# 8. CONSUMER_GROUP

## Purpose

A CONSUMER_GROUP tracks stream readers and their offsets.

Allows multiple applications to consume the same stream independently.

---

### Example

```sql
CREATE CONSUMER_GROUP dashboard_service;
```

---

### Metadata

```text
consumer_group_id
group_name
offsets
members
lag
status
```

---

### Responsibilities

* Offset tracking
* Load balancing
* Recovery
* Fault tolerance

---

### Example

```sql
SUBSCRIBE orders
CONSUMER_GROUP analytics_service;
```

---

# 9. WINDOW

## Purpose

A WINDOW defines event aggregation boundaries.

Used for real-time analytics.

---

### Window Types

#### Tumbling Window

```text
0-5 min
5-10 min
10-15 min
```

---

#### Sliding Window

```text
Every Minute
Previous 5 Minutes
```

---

#### Session Window

```text
User Activity Sessions
```

---

### Example

```sql
CREATE WINDOW last_hour
TYPE SLIDING
SIZE 1 HOUR;
```

---

### Example Query

```sql
SELECT
    COUNT(*)
FROM orders
WINDOW last_hour;
```

---

# 10. STREAM_PROCESSOR

## Purpose

A STREAM_PROCESSOR executes transformations on streams.

Acts as Mnemosyne's equivalent of stream jobs.

---

### Example

```sql
CREATE STREAM_PROCESSOR order_enrichment;
```

---

### Responsibilities

* Filtering
* Mapping
* Aggregation
* Joining
* Enrichment

---

### Example

```sql
CREATE STREAM_PROCESSOR fraud_detector
AS
SELECT *
FROM transactions
WHERE amount > 10000;
```

---

### Output

Can write to:

* Streams
* Tables
* Materialized Views
* Models

---

# 11. EVENT_TRIGGER

## Purpose

Executes actions when stream conditions occur.

Provides event-driven automation.

---

### Example

```sql
CREATE EVENT_TRIGGER high_risk_transaction;
```

---

### Example Condition

```sql
WHEN amount > 50000
```

---

### Actions

```text
Run Pipeline
Call Model
Send Notification
Create Alert
Invoke Webhook
```

---

### Example

```sql
CREATE EVENT_TRIGGER churn_prediction
WHEN customer_activity_drops;
```

---

# 12. Stream Persistence

Streams support:

## Memory Only

```text
Fastest
Volatile
```

---

## Local Storage

```text
Persistent
Single Node
```

---

## Object Storage

```text
S3
MinIO
Ceph
```

---

### Example

```sql
CREATE STREAM user_events
STORAGE_UNIT streaming_store;
```

---

# 13. Partitioning

Streams integrate with:

```sql
SHARD_GROUP
```

Example:

```sql
CREATE STREAM user_events
SHARD_GROUP user_distribution;
```

---

### Benefits

* Horizontal scalability
* Parallel processing
* Load balancing

---

# 14. Replication

Streams integrate with:

```sql
REPLICA_GROUP
```

Example:

```sql
CREATE STREAM user_events
REPLICA_GROUP standard_ha;
```

---

### Benefits

* Durability
* Fault tolerance
* Failover

---

# 15. Stream States

```text
CREATING
ACTIVE
PAUSED
DEGRADED
OFFLINE
ARCHIVED
```

---

# 16. Retention Policies

Examples:

```sql
RETAIN 7 DAYS;
```

```sql
RETAIN 30 DAYS;
```

```sql
RETAIN FOREVER;
```

---

# 17. Replay Support

Allows historical event reprocessing.

Example:

```sql
REPLAY STREAM user_events
FROM '2026-01-01';
```

Use Cases:

* Backfills
* Model retraining
* Auditing

---

# 18. Exactly Once Processing

Mnemosyne supports:

## At Most Once

```text
Fastest
Possible Loss
```

---

## At Least Once

```text
No Loss
Possible Duplicates
```

---

## Exactly Once

```text
No Loss
No Duplicates
```

Recommended for financial systems.

---

# 19. Integration with Mnemosyne Objects

Streams may feed:

## Tables

```sql
STREAM → TABLE
```

---

## Materialized Views

```sql
STREAM → MATERIALIZED_VIEW
```

---

## Pipelines

```sql
STREAM → PIPELINE
```

---

## Models

```sql
STREAM → MODEL
```

---

## Metrics

```sql
STREAM → METRIC
```

---

## Alerts

```sql
STREAM → ALERT
```

---

# 20. Monitoring

Metrics:

```text
Events Per Second
Consumer Lag
Partition Health
Throughput
Latency
Retention Usage
Replay Activity
```

Commands:

```sql
SHOW STREAMS;
SHOW TOPICS;
SHOW CONSUMER_GROUPS;
SHOW STREAM_METRICS;
```

---

# 21. Security

Permissions:

```text
CREATE_STREAM
READ_STREAM
WRITE_STREAM
ALTER_STREAM
DROP_STREAM
```

Additional controls:

* Encryption at rest
* TLS
* mTLS
* Stream-level ACLs

---

# 22. Future Roadmap

## Phase 1

* Streams
* Topics
* Consumer Groups

---

## Phase 2

* Windowing
* Stream Processors

---

## Phase 3

* Event Triggers
* Exactly Once Processing

---

## Phase 4

* Distributed Stream Processing

---

## Phase 5

* AI-Powered Stream Analytics

---

## Phase 6

* Autonomous Event Routing

---

# 23. Success Metrics

* Event throughput
* Stream latency
* Consumer lag
* Replay reliability
* Processing efficiency
* Stream availability

---

# 24. Long-Term Vision

The Mnemosyne Streaming Layer becomes the real-time nervous system of Core&Outline.

Every click, transaction, sensor reading, model prediction, recommendation, dashboard update, and business event flows through streams.

The Streaming Layer enables Mnemosyne to unify operational data, analytical processing, machine learning, and decision intelligence into a single distributed platform without requiring external messaging systems.
