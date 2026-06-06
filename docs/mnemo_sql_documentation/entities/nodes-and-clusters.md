# Nodes and Clusters

Distributed Mnemo deployments register **nodes** (workers, coordinators) and optional **clusters**.

## CREATE NODE

```sql
CREATE NODE worker_01 TYPE COMPUTE ROLE WORKER;
CREATE NODE coord_01 TYPE HYBRID ROLE COORDINATOR;
```

Node types (examples): `COMPUTE`, `HYBRID`, `GPU`  
Roles (examples): `WORKER`, `COORDINATOR`, `OBSERVER`

## REGISTER NODE

Register network location (alternative to HTTP register API):

```sql
REGISTER NODE worker_02 HOST '127.0.0.1' PORT 9001;
```

## DRAIN / REMOVE

```sql
DRAIN NODE worker_01;
REMOVE NODE worker_01;
REMOVE NODE IF EXISTS worker_01;
```

## CREATE CLUSTER

```sql
CREATE CLUSTER main;
CREATE CLUSTER IF NOT EXISTS production;
```

## ALTER NODE

```sql
ALTER NODE worker_01 SET ROLE WORKER;
ALTER NODE worker_01 SET TYPE COMPUTE;
```

## SHOW

```sql
SHOW NODES;
SHOW NODE METRICS worker_01;
SHOW NODE CAPABILITIES worker_01;
SHOW NODE PARTITIONS worker_01;
SHOW NODE REPLICAS worker_01;
SHOW CLUSTERS;
```

## DESCRIBE

```sql
DESCRIBE NODE worker_02;
DESCRIBE CLUSTER main;
```

Every server typically lists a local **self** node on `SHOW NODES` (type `HYBRID`, role `COORDINATOR`, status `ONLINE` in tests).

See `scripts/test_nodes_api.py`.
