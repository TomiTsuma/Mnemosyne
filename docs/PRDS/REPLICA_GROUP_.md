# Product Requirements Document (PRD)

# REPLICA_GROUP First-Class Entity

**Product:** Mnemosyne Data Platform
**Component:** Distributed Reliability Layer
**Version:** 1.0
**Status:** Phase 1 Implemented

**Implementation notes:** See `docs/changelog/060626-replica_group_entity_changelog.md`.
Phase 1 ships catalog + SQL + node placement + failover metadata + monitoring.
Physical replication and quorum writes are deferred to Phase 2.

---

# 1. Overview

## Purpose

The REPLICA_GROUP entity defines how Mnemosyne replicates data and workloads across nodes within a cluster.

A replica group specifies:

* Replication factor
* Replica placement rules
* Failover behavior
* Consistency policies
* Recovery policies

Replica groups provide the foundation for:

* High availability
* Fault tolerance
* Disaster recovery
* Load distribution

---

# 2. Vision

Mnemosyne should enable users to define replication once and reuse it across multiple Mnemosyne objects.

Instead of:

```sql
CREATE TABLE sales
REPLICATION_FACTOR 3;
```

Mnemosyne uses:

```sql
CREATE REPLICA_GROUP standard_ha
REPLICAS 3;
```

Then:

```sql
CREATE TABLE sales
REPLICA_GROUP standard_ha;
```

This creates centralized replication governance.

---

# 3. Goals

## Functional Goals

* Define reusable replication policies.
* Support multiple replication strategies.
* Enable automatic failover.
* Support workload distribution.
* Support future multi-region deployments.

## Non-Functional Goals

* High availability
* Durability
* Scalability
* Operational simplicity

---

# 4. Core Concepts

A replica group defines a set of replicas managed together.

Example:

```sql
CREATE REPLICA_GROUP standard_ha
REPLICAS 3;
```

Internally:

```text
Primary
 ├── Replica A
 └── Replica B
```

The replica group governs synchronization and recovery.

---

# 5. Objects Supporting Replica Groups

Replica groups may be attached to:

## Tables

```sql
CREATE TABLE sales
REPLICA_GROUP standard_ha;
```

---

## Streams

```sql
CREATE STREAM user_events
REPLICA_GROUP stream_ha;
```

---

## Indexes

```sql
CREATE INDEX customer_idx
REPLICA_GROUP search_ha;
```

---

## Vector Indexes

```sql
CREATE VECTOR_INDEX embeddings
REPLICA_GROUP vector_ha;
```

---

## Models

Future support.

```sql
CREATE MODEL churn_model
REPLICA_GROUP model_ha;
```

---

# 6. Replica Group Metadata

Each replica group stores:

```text
replica_group_id
name
replication_factor
strategy
consistency_mode
status
created_at
updated_at
owner
```

---

# 7. Replication Factor

Defines total copies.

Example:

```sql
REPLICAS 3;
```

Result:

```text
1 Primary
2 Replicas
```

---

Examples:

```text
REPLICAS 1
No redundancy
```

```text
REPLICAS 2
One backup
```

```text
REPLICAS 3
Production recommendation
```

---

# 8. Replication Strategies

## PRIMARY_REPLICA

Single writable primary.

Example:

```text
Primary
 ├── Replica A
 └── Replica B
```

Recommended for MVP.

---

## MULTI_PRIMARY

Multiple writable replicas.

Example:

```text
Primary A
Primary B
Primary C
```

Requires conflict resolution.

Future capability.

---

## OBSERVER

Read-only replicas.

Example:

```text
Primary
 ├── Replica
 └── Observer
```

Useful for analytics and monitoring.

---

# 9. Consistency Modes

## SYNCHRONOUS

Write flow:

```text
Write
 ↓
All replicas acknowledge
 ↓
Commit
```

Benefits:

* Strong consistency

Tradeoff:

* Higher latency

---

## ASYNCHRONOUS

Write flow:

```text
Write
 ↓
Primary commits
 ↓
Replicas update later
```

Benefits:

* High performance

Tradeoff:

* Potential lag

---

## QUORUM

Write flow:

```text
Write
 ↓
Majority acknowledge
 ↓
Commit
```

Benefits:

* Balance of consistency and availability

Recommended long-term default.

---

# 10. Placement Policies

Replica groups control replica placement.

## NODE_AWARE

Replicas placed on different nodes.

Example:

```text
Node A
Node B
Node C
```

---

## RACK_AWARE

Future capability.

Replicas distributed across racks.

---

## REGION_AWARE

Future capability.

Replicas distributed across regions.

Example:

```text
US-East
EU-West
AP-Southeast
```

---

# 11. Failover

Replica groups automatically recover from failures.

Example:

```text
Primary Fails
 ↓
Replica Promoted
 ↓
Traffic Redirected
```

---

Failover requirements:

* Automatic detection
* Promotion
* Metadata updates
* Client transparency

---

# 12. Recovery

When failed nodes return:

```text
Node Returns
 ↓
Catch Up
 ↓
Rejoin Replica Group
```

Mnemosyne automatically synchronizes missing data.

---

# 13. Replica States

Supported states:

```text
CREATING
SYNCING
ONLINE
DEGRADED
OFFLINE
PROMOTING
RECOVERING
```

---

# 14. Replication Monitoring

Mnemosyne tracks:

```text
Replication Lag
Replica Health
Sync Progress
Recovery Status
Failover Events
```

Commands:

```sql
SHOW REPLICA_GROUPS;
```

```sql
SHOW REPLICATION_STATUS;
```

---

# 15. Stream Replication

Stream partitions use replica groups.

Example:

```text
Partition 0
 ├── Primary
 ├── Replica A
 └── Replica B
```

Benefits:

* No event loss
* Fast recovery

---

# 16. Table Replication

Table partitions use replica groups.

Example:

```text
Sales Partition 17
 ├── Primary
 ├── Replica A
 └── Replica B
```

---

# 17. Pipeline Replication

Future capability.

Pipeline metadata replicated across nodes.

Benefits:

* Pipeline recovery
* Scheduler recovery

---

# 18. Model Replication

Future capability.

Replicate:

* Model artifacts
* Feature metadata
* Inference endpoints

Benefits:

* High availability AI services

---

# 19. Security

Permissions:

```text
CREATE_REPLICA_GROUP
ALTER_REPLICA_GROUP
DROP_REPLICA_GROUP
VIEW_REPLICA_GROUP
```

Replication traffic supports:

```text
TLS
mTLS
Encryption at Rest
```

---

# 20. SQL Interface

## Create

```sql
CREATE REPLICA_GROUP standard_ha
REPLICAS 3
CONSISTENCY QUORUM;
```

---

## Show

```sql
SHOW REPLICA_GROUPS;
```

---

## Describe

```sql
DESCRIBE REPLICA_GROUP standard_ha;
```

---

## Alter

```sql
ALTER REPLICA_GROUP standard_ha
SET REPLICAS 5;
```

---

## Drop

```sql
DROP REPLICA_GROUP standard_ha;
```

---

# 21. Example Policies

## Development

```sql
CREATE REPLICA_GROUP dev
REPLICAS 1;
```

No redundancy.

---

## Standard Production

```sql
CREATE REPLICA_GROUP standard_ha
REPLICAS 3
CONSISTENCY QUORUM;
```

Recommended default.

---

## High Durability

```sql
CREATE REPLICA_GROUP critical_data
REPLICAS 5
CONSISTENCY SYNCHRONOUS;
```

Used for:

* Financial data
* Audit logs
* Compliance systems

---

# 22. Future Roadmap

Phase 1

* Primary-replica architecture
* Automatic failover
* Replica monitoring

Phase 2

* Quorum consistency
* Replica balancing

Phase 3

* Multi-region replication
* Cross-cluster replication

Phase 4

* Multi-primary replication
* Conflict resolution

Phase 5

* Autonomous replica placement
* Predictive failover

---

# 23. Success Metrics

* Replica availability
* Recovery time objective (RTO)
* Replication lag
* Failover duration
* Data durability
* Cluster uptime

---

# 24. Vision

The REPLICA_GROUP entity provides a reusable, policy-driven replication framework for Mnemosyne. By separating replication policies from storage objects, Mnemosyne enables consistent high-availability, fault tolerance, and disaster recovery across tables, streams, indexes, vector stores, models, and future platform capabilities.

Replica groups become the foundational durability and availability abstraction for the Mnemosyne distributed architecture.
