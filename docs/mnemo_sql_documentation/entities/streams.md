# Streams

The streaming layer provides append-only **topics**, **streams** that bind to topics, and **consumer groups** for offset tracking. Use `PUBLISH` and `SUBSCRIBE` for event-driven pipelines.

## Entity relationships

```
TOPIC (partitioned log)
  ↑ PUBLISH
STREAM (consumer-facing binding, optional RETAIN)
  ↓ SUBSCRIBE
CONSUMER_GROUP (offset cursor per stream)
```

## TOPIC

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Topic identifier |
| `partition_count` | uint32 | Number of partitions |
| `retention` | enum | `DAYS`, `FOREVER` |
| `retention_days` | uint32 | Days to retain (when `DAYS`) |
| `created_at` | timestamp | Creation time |

### CREATE TOPIC

```sql
CREATE TOPIC user_activity PARTITIONS 4 RETAIN 30 DAYS;
CREATE TOPIC audit_log PARTITIONS 8 RETAIN FOREVER;
CREATE TOPIC orders PARTITIONS 16 RETAIN 7 DAYS;
```

| Clause | Meaning |
|--------|---------|
| `PARTITIONS n` | Partition count for parallelism |
| `RETAIN n DAYS` | Expire events after n days |
| `RETAIN FOREVER` | No automatic expiry |

## STREAM

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Stream identifier |
| `topic_name` | string | Bound topic (optional) |
| `status` | enum | `CREATING`, `ACTIVE`, `PAUSED`, `DEGRADED`, `OFFLINE`, `ARCHIVED` |
| `retention` | enum | Stream-level retention override |
| `retention_days` | uint32 | Days (when `DAYS`) |
| `created_at` | timestamp | Creation time |

### CREATE STREAM

```sql
CREATE STREAM user_events TOPIC user_activity RETAIN 7 DAYS;
CREATE STREAM orders_stream TOPIC orders RETAIN 30 DAYS;
CREATE STREAM standalone_stream;   -- no topic binding
```

## CONSUMER_GROUP

### Properties

| Property | Description |
|----------|-------------|
| `name` | Consumer group identifier |
| `offsets` | Map stream/topic → last consumed offset |
| `created_at` | Creation time |

### CREATE CONSUMER GROUP

```sql
CREATE CONSUMER_GROUP analytics;
CREATE CONSUMER_GROUP dashboard;
CREATE CONSUMER_GROUP fraud_detection;
```

## PUBLISH

Append events to a topic:

```sql
PUBLISH user_activity VALUES ('user-1', 'click', '2025-01-15');
PUBLISH user_activity VALUES
    ('user-1', 'click', '2025-01-15 10:00:00'),
    ('user-2', 'purchase', '2025-01-15 10:05:00');
```

Syntax mirrors `INSERT` — multiple tuples in one statement.

## SUBSCRIBE

Read events for a consumer group:

```sql
SUBSCRIBE user_events CONSUMER_GROUP analytics LIMIT 100;
SUBSCRIBE orders_stream CONSUMER_GROUP dashboard LIMIT 50;
```

Returns up to `LIMIT` events and advances the consumer group offset.

### StreamEvent fields

| Field | Description |
|-------|-------------|
| `offset` | Event offset in log |
| `payload` | Event body (tuple values) |
| `ts` | Event timestamp |

## ALTER STREAM

```sql
ALTER STREAM user_events SET RETENTION 14 DAYS;
ALTER STREAM user_events SET RETENTION FOREVER;
```

## SHOW

```sql
SHOW TOPICS;
SHOW STREAMS;
SHOW CONSUMER_GROUPS;
SHOW STREAM METRICS FOR STREAM user_events;
```

### Stream metrics

| Field | Description |
|-------|-------------|
| `event_count` | Total events |
| `consumer_lag` | Lag for consumer group |
| `consumer_group` | Group being measured |

## DESCRIBE / DROP

```sql
DESCRIBE TOPIC user_activity;
DESCRIBE STREAM user_events;
DESCRIBE CONSUMER_GROUP analytics;

DROP TOPIC user_activity;
DROP STREAM user_events;
DROP CONSUMER_GROUP analytics;

DROP STREAM IF EXISTS old_stream;
```

## End-to-end example

```sql
USE streams_test_db;

CREATE TOPIC api_user_activity PARTITIONS 4 RETAIN 30 DAYS;
CREATE STREAM api_user_events TOPIC api_user_activity RETAIN 7 DAYS;
CREATE CONSUMER_GROUP api_analytics;

PUBLISH api_user_activity VALUES ('evt-1', 'login', 'user-42');
PUBLISH api_user_activity VALUES ('evt-2', 'click', 'user-42');

SUBSCRIBE api_user_events CONSUMER_GROUP api_analytics LIMIT 10;

SHOW STREAM METRICS FOR STREAM api_user_events;
```

## Python example

```python
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
client.query("CREATE DATABASE IF NOT EXISTS streams_test_db")
client.query("USE streams_test_db")

client.query("CREATE TOPIC api_user_activity PARTITIONS 4 RETAIN 30 DAYS")
client.query("CREATE STREAM api_user_events TOPIC api_user_activity RETAIN 7 DAYS")
client.query("CREATE CONSUMER_GROUP api_analytics")

r = client.query("PUBLISH api_user_activity VALUES ('evt-1', 'click', 'user-1')")
assert r.ok()

r = client.query("SUBSCRIBE api_user_events CONSUMER_GROUP api_analytics LIMIT 10")
assert r.ok()
assert "evt-1" in r.body or len(r.json().get("data", [])) >= 1

r = client.query("DESCRIBE TOPIC api_user_activity")
assert "4" in r.body  # partition_count
```

## Streaming + pipeline pattern

Ingest events, aggregate into tables, refresh dashboards:

```sql
-- Pipeline task: drain stream into table
CREATE TASK ingest_events IN STAGE load IN PIPELINE realtime
    TYPE SQL
    BODY 'CREATE TABLE IF NOT EXISTS event_log (payload String) ENGINE=Memory';

-- Application loop (pseudo):
-- SUBSCRIBE user_events CONSUMER_GROUP etl LIMIT 1000;
-- INSERT INTO event_log SELECT ... from subscribe results
```

## Streaming + ML pattern

Publish features for online scoring:

```sql
PUBLISH feature_updates VALUES ('customer-42', 'tenure=12', 'charges=89.5');

-- Downstream service SUBSCRIBE + PREDICT MODEL churn_model:1 WITH (...)
```

## Related

- [pipelines.md](pipelines.md) — batch complement to streaming
- [../model-layer/visualizations.md](../model-layer/visualizations.md) — stream metrics dashboards
- `scripts/test_streams_api.py`
