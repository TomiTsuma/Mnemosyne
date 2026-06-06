# Product Requirements Document (PRD)

# NODE First-Class Entity

**Product:** Mnemosyne Data Platform
**Component:** Distributed Infrastructure Layer
**Version:** 1.0
**Status:** Draft

---

# 1. Overview

## Purpose

The NODE entity represents a running Mnemosyne runtime instance participating in a cluster.

Nodes provide the computational, storage, networking, streaming, and AI execution capabilities of Mnemosyne.

All distributed operations within Mnemosyne ultimately execute on one or more nodes.

Examples include:

* Query execution
* Table storage
* Stream processing
* Pipeline execution
* Model training
* Model inference
* Search indexing
* Graph processing

---

# 2. Vision

Mnemosyne must scale from:

```text
Single Laptop
```

to

```text
Hundreds of Distributed Servers
```

without changing the logical data model.

Nodes provide the fundamental execution unit that enables horizontal scaling, workload distribution, fault tolerance, and resource isolation.

---

# 3. Goals

## Functional Goals

* Represent Mnemosyne runtime instances.
* Participate in cluster membership.
* Execute workloads.
* Store partitions.
* Process streams.
* Run pipelines.
* Execute machine learning workloads.
* Report health and metrics.

## Non-Functional Goals

* High availability
* Elastic scaling
* Efficient resource utilization
* Fault tolerance
* Observability

---

# 4. Core Concepts

A node is a running Atlas process.

Example:

```sql
CREATE NODE worker_01;
```

Internally:

```text
worker_01
 ├── Query Engine
 ├── Storage Engine
 ├── Stream Engine
 ├── Pipeline Engine
 ├── Model Runtime
 └── Control Agent
```

---

# 5. Node Responsibilities

Nodes execute workloads assigned by the cluster.

Responsibilities include:

### Query Execution

Executing SQL queries.

### Storage Management

Managing partitions and replicas.

### Stream Processing

Processing stream partitions.

### Pipeline Execution

Executing pipeline tasks.

### AI Execution

Training and serving models.

### Monitoring

Reporting metrics and health.

---

# 6. Node Metadata

Every node maintains metadata.

Schema:

```text
node_id
node_name
cluster_id
node_type
node_role
status
host
port
version
created_at
updated_at
```

---

# 7. Node Types

## COMPUTE

Executes workloads.

Stores little or no data.

Responsibilities:

* Query execution
* Pipeline execution
* Model execution

Example:

```text
Compute Node
```

---

## STORAGE

Stores data partitions.

Responsibilities:

* Table storage
* Stream storage
* Index storage

Example:

```text
Storage Node
```

---

## HYBRID

Executes workloads and stores data.

Recommended for:

* Development
* Homelabs
* Small deployments

---

## GPU

Specialized AI node.

Responsibilities:

* Model training
* Embedding generation
* Inference

Example:

```text
GPU Node
```

---

# 8. Node Roles

Roles determine cluster responsibilities.

## COORDINATOR

Responsible for:

* Scheduling
* Metadata coordination
* Query planning

Typically few per cluster.

---

## WORKER

Responsible for:

* Execution
* Storage
* Streaming
* Pipelines

Most common role.

---

## OBSERVER

Read-only participation.

Use Cases:

* Monitoring
* Disaster recovery

---

# 9. Resource Management

Nodes expose available resources.

Tracked resources:

```text
CPU
Memory
GPU
Storage
Network
```

Example:

```sql
DESCRIBE NODE worker_01;
```

Output:

```text
CPU: 16
Memory: 64 GB
GPU: 1
Storage: 4 TB
```

---

# 10. Node Capabilities

Nodes advertise supported capabilities.

Examples:

```text
QUERY_ENGINE
COLUMNAR_STORAGE
ROW_STORAGE
STREAMING
PIPELINES
SEARCH
GRAPH
VECTOR
ML_TRAINING
ML_INFERENCE
```

Example:

```sql
SHOW NODE CAPABILITIES;
```

---

# 11. Heartbeat System

Nodes periodically send heartbeats.

Example:

```text
Every 5 Seconds
```

Reported information:

```text
Status
Resource Usage
Latency
Version
Errors
```

Purpose:

* Failure detection
* Scheduling decisions
* Capacity planning

---

# 12. Node States

Supported states:

```text
REGISTERING
ONLINE
BUSY
DEGRADED
OFFLINE
MAINTENANCE
DRAINING
```

---

## DRAINING

Node is being removed.

Example:

```text
Move workloads
Move replicas
Remove node
```

Used for rolling upgrades.

---

# 13. Storage Responsibilities

Storage nodes may own:

```text
Partitions
Replicas
Indexes
Stream Segments
Model Artifacts
```

Example:

```text
sales_partition_17
```

owned by:

```text
worker_03
```

---

# 14. Stream Responsibilities

Nodes process stream partitions.

Example:

```text
user_events
 ├── Partition 0
 ├── Partition 1
 └── Partition 2
```

Assignments:

```text
Node A → Partition 0
Node B → Partition 1
Node C → Partition 2
```

---

# 15. Pipeline Responsibilities

Nodes execute pipeline stages.

Example:

```text
Read
 ↓
Transform
 ↓
Aggregate
 ↓
Write
```

Execution may be distributed across multiple nodes.

---

# 16. AI Responsibilities

Nodes may execute:

## Training

```text
Forecasting
Classification
Recommendation
```

---

## Inference

```text
Predictions
Embeddings
Scoring
```

---

## LLM Workloads

Future support:

```text
Agents
RAG
Reasoning
```

---

# 17. Scheduling

The cluster scheduler places workloads using:

```text
Resource Availability
Data Locality
Node Health
Node Capabilities
Priority
```

Example:

```text
Query
 ↓
Best Node Selected
 ↓
Execution
```

---

# 18. Networking

Nodes communicate through an internal control plane.

Functions:

* Membership updates
* Replication
* Scheduling
* Metadata synchronization

Protocols:

```text
gRPC
TCP
TLS
```

---

# 19. Fault Tolerance

Node failures must be detected automatically.

Failure flow:

```text
Heartbeat Lost
 ↓
Node Marked Offline
 ↓
Workload Reassignment
 ↓
Replica Promotion
```

---

# 20. Replication Participation

Nodes may host:

```text
Primary Partitions
Secondary Replicas
```

Example:

```text
Node A
Primary

Node B
Replica

Node C
Replica
```

---

# 21. Security

Node authentication:

```text
TLS
mTLS
Certificates
```

Supported permissions:

```text
CREATE_NODE
ALTER_NODE
REMOVE_NODE
VIEW_NODE
```

---

# 22. Monitoring

Node metrics:

```text
CPU Utilization
Memory Usage
Storage Usage
Network Throughput
Query Throughput
Pipeline Throughput
Stream Throughput
```

Commands:

```sql
SHOW NODES;
```

```sql
SHOW NODE METRICS;
```

---

# 23. SQL Interface

## Create Node

```sql
CREATE NODE worker_01;
```

---

## Register Existing Node

```sql
REGISTER NODE worker_01
HOST '10.0.0.12'
PORT 9000;
```

---

## Show Nodes

```sql
SHOW NODES;
```

---

## Describe Node

```sql
DESCRIBE NODE worker_01;
```

---

## Alter Node

```sql
ALTER NODE worker_01
SET ROLE WORKER;
```

---

## Drain Node

```sql
DRAIN NODE worker_01;
```

---

## Remove Node

```sql
REMOVE NODE worker_01;
```

---

# 24. Future Roadmap

Phase 1

* Single-node operation
* Membership tracking
* Health monitoring

Phase 2

* Distributed execution
* Resource scheduling

Phase 3

* Stream processing
* Pipeline execution

Phase 4

* Replication ownership
* High availability

Phase 5

* GPU scheduling
* Distributed training

Phase 6

* Autonomous workload placement
* Self-healing infrastructure

---

# 25. Success Metrics

* Node uptime
* Resource utilization
* Query throughput
* Stream throughput
* Pipeline throughput
* Failure recovery time
* Scheduling efficiency

---

# 26. Vision

The NODE entity serves as the fundamental execution unit of Mnemosyne. Nodes provide the compute, storage, streaming, AI, and orchestration capabilities required to operate a distributed data and intelligence platform.

By abstracting execution into nodes, Mnemosyne can scale seamlessly from a single-machine deployment to a globally distributed infrastructure while maintaining a consistent user-facing experience.
