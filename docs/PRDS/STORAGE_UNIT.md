# Product Requirements Document (PRD)

# STORAGE_UNIT First-Class Entity

**Product:** Atlas Data Platform
**Component:** Storage Management Layer
**Version:** 1.0
**Status:** Draft

---

# 1. Overview

## Purpose

The STORAGE_UNIT entity provides a logical abstraction over physical storage resources used by Atlas.

Rather than hard-coding storage paths or object storage buckets throughout the platform, Atlas introduces STORAGE_UNIT as a first-class database object that encapsulates storage configuration, lifecycle management, security policies, and storage capabilities.

A STORAGE_UNIT serves as the foundation upon which databases, tables, streams, indexes, models, and other Atlas objects persist their data.

---

# 2. Motivation

Modern data platforms utilize multiple storage technologies simultaneously:

* Local SSDs
* Network Attached Storage (NAS)
* Amazon S3
* MinIO
* Google Cloud Storage
* Azure Blob Storage
* Ceph
* HDFS

Atlas requires a unified abstraction that allows storage resources to be managed consistently regardless of underlying technology.

The STORAGE_UNIT entity solves:

* Multi-cloud storage support
* Hybrid cloud deployments
* Tiered storage
* Data lifecycle management
* Storage isolation between tenants
* Storage monitoring and governance

---

# 3. Goals

## Functional Goals

* Provide a unified storage abstraction.
* Support local and remote storage backends.
* Support multiple storage units within a single Atlas deployment.
* Allow databases and tables to select storage units.
* Enable tiered storage strategies.
* Support future replication policies.
* Support future backup policies.
* Support future encryption policies.

## Non-Functional Goals

* High availability
* Horizontal scalability
* Storage backend independence
* Extensibility
* Low-latency metadata operations

---

# 4. Core Concepts

## Storage Unit

A STORAGE_UNIT represents a physical or logical storage target.

Examples:

### Local SSD

```sql
CREATE STORAGE_UNIT local_ssd
TYPE LOCAL
PATH '/atlas/data';
```

### MinIO

```sql
CREATE STORAGE_UNIT minio_store
TYPE S3
ENDPOINT 'https://minio.company.com'
BUCKET 'atlas';
```

### Amazon S3

```sql
CREATE STORAGE_UNIT s3_warehouse
TYPE S3
BUCKET 'atlas-production';
```

---

# 5. Supported Storage Types

## LOCAL

Single-node filesystem storage.

Properties:

* Path
* Capacity
* Available Space

Use Cases:

* Development
* Edge deployments
* Homelabs

---

## S3

S3-compatible object storage.

Supports:

* AWS S3
* MinIO
* Ceph
* Wasabi
* Cloudflare R2

Properties:

* Endpoint
* Bucket
* Region
* Access Key
* Secret Key

Use Cases:

* Data lakes
* Long-term storage
* Backups

---

## HDFS

Distributed filesystem.

Properties:

* Namenode
* Replication Factor

Use Cases:

* Large-scale distributed deployments

---

## NFS

Network file systems.

Properties:

* Mount Point
* Server

Use Cases:

* Shared storage clusters

---

# 6. Storage Unit Metadata

Each STORAGE_UNIT maintains metadata.

Schema:

```text
storage_unit_id
name
type
status
capacity_bytes
used_bytes
available_bytes
created_at
updated_at
owner
```

Example:

```sql
SHOW STORAGE_UNITS;
```

Output:

```text
local_ssd       ONLINE
minio_store     ONLINE
s3_archive      ONLINE
```

---

# 7. Storage Classes

Atlas supports storage classes.

## HOT

Fastest storage.

Examples:

* NVMe SSD
* High-performance object storage

Use Cases:

* Active tables
* Recent events
* Indexes

---

## WARM

Moderate performance.

Use Cases:

* Historical analytics
* Aggregated datasets

---

## COLD

Lowest-cost storage.

Examples:

* S3 Glacier
* Archive storage

Use Cases:

* Compliance archives
* Long-term retention

---

# 8. Storage Policies

## Placement Policy

Determines where data is stored.

Example:

```sql
CREATE TABLE events
STORAGE_UNIT hot_ssd;
```

---

## Tiering Policy

Automatically moves data.

Example:

```text
0-30 days    HOT
31-180 days  WARM
181+ days    COLD
```

---

## Retention Policy

Controls expiration.

Example:

```sql
RETENTION 365 DAYS;
```

---

# 9. Storage Unit Capabilities

Storage units expose capabilities.

Examples:

```text
OBJECT_STORAGE
BLOCK_STORAGE
STREAMING
VERSIONING
ENCRYPTION
REPLICATION
SNAPSHOTS
```

Example:

```sql
SHOW STORAGE_UNIT CAPABILITIES;
```

---

# 10. Atlas Objects Using Storage Units

The following Atlas objects may reference a STORAGE_UNIT.

## Database

```sql
CREATE DATABASE analytics
STORAGE_UNIT warehouse_store;
```

---

## Table

```sql
CREATE TABLE sales
STORAGE_UNIT hot_ssd;
```

---

## Stream

```sql
CREATE STREAM user_events
STORAGE_UNIT event_log_store;
```

---

## Index

```sql
CREATE INDEX customer_idx
STORAGE_UNIT fast_index_store;
```

---

## Model

```sql
CREATE MODEL churn_predictor
STORAGE_UNIT model_store;
```

---

# 11. Tiered Storage Architecture

Atlas supports automatic data movement.

```text
Hot SSD
    ↓
Warm Object Store
    ↓
Cold Archive
```

Example:

```sql
CREATE TIER_POLICY sales_policy
HOT 30 DAYS
WARM 180 DAYS
COLD INDEFINITE;
```

---

# 12. Monitoring

Atlas continuously monitors:

* Capacity
* Throughput
* Latency
* Error Rates
* Replication Health

Commands:

```sql
SHOW STORAGE_USAGE;
```

```sql
SHOW STORAGE_METRICS;
```

---

# 13. Security

Supported controls:

## Encryption at Rest

```sql
ENCRYPTION AES256;
```

---

## Encryption in Transit

TLS support for remote stores.

---

## Access Control

Permissions:

```text
CREATE_STORAGE_UNIT
ALTER_STORAGE_UNIT
DROP_STORAGE_UNIT
READ_STORAGE_UNIT
```

---

# 14. Replication

Future capability.

Example:

```sql
CREATE STORAGE_UNIT warehouse
TYPE S3
REPLICATE TO backup_store;
```

Benefits:

* Disaster recovery
* Multi-region availability

---

# 15. Backup & Snapshot Support

Future capability.

Commands:

```sql
CREATE SNAPSHOT analytics_snapshot;
```

```sql
RESTORE SNAPSHOT analytics_snapshot;
```

---

# 16. Failure Handling

Storage unit states:

```text
ONLINE
DEGRADED
OFFLINE
MAINTENANCE
```

Atlas automatically reroutes workloads where possible.

---

# 17. SQL Interface

## Create

```sql
CREATE STORAGE_UNIT warehouse
TYPE S3
BUCKET 'atlas-prod';
```

## Show

```sql
SHOW STORAGE_UNITS;
```

## Describe

```sql
DESCRIBE STORAGE_UNIT warehouse;
```

## Alter

```sql
ALTER STORAGE_UNIT warehouse
SET RETENTION 365 DAYS;
```

## Drop

```sql
DROP STORAGE_UNIT warehouse;
```

---

# 18. Future Roadmap

Phase 1:

* Local storage
* S3-compatible storage
* Capacity monitoring

Phase 2:

* Tiering policies
* Replication
* Lifecycle management

Phase 3:

* Multi-region storage
* Storage federation
* Intelligent data placement

Phase 4:

* AI-driven storage optimization
* Autonomous tiering
* Cost-aware workload placement

---

# 19. Success Metrics

* Storage units created successfully
* Storage utilization tracked accurately
* Data placement policy compliance
* Tiering efficiency
* Recovery time objectives achieved
* Storage cost reduction through lifecycle policies

---

# 20. Vision

STORAGE_UNIT becomes the foundational storage abstraction for Atlas, enabling every data object—from tables and streams to models and vector indexes—to operate independently of the underlying storage technology while supporting scalable, hybrid, and cloud-native deployments.
