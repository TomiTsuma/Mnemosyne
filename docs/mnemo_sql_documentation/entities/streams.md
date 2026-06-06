# Streams

Streaming layer: **topics** hold partitioned event logs; **streams** consume topics; **consumer groups** track offsets.

## CREATE TOPIC

```sql
CREATE TOPIC user_activity PARTITIONS 4 RETAIN 30 DAYS;
CREATE TOPIC audit_log PARTITIONS 8 RETAIN FOREVER;
```

| Clause | Meaning |
|--------|---------|
| `PARTITIONS n` | Partition count |
| `RETAIN n DAYS` | Retention window |
| `RETAIN FOREVER` | No expiry |

## CREATE STREAM

```sql
CREATE STREAM user_events TOPIC user_activity RETAIN 7 DAYS;

CREATE STREAM orders_stream;   -- optional topic binding
```

## CREATE CONSUMER GROUP

```sql
CREATE CONSUMER_GROUP analytics;
CREATE CONSUMER_GROUP dashboard;
```

## PUBLISH

Publish events to a topic:

```sql
PUBLISH user_activity VALUES ('user-1', 'click', '2025-01-15');
```

Multiple value tuples supported like `INSERT`.

## SUBSCRIBE

```sql
SUBSCRIBE user_events CONSUMER_GROUP analytics LIMIT 100;
```

Reads up to `LIMIT` events for the consumer group.

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

## DESCRIBE / DROP

```sql
DESCRIBE TOPIC user_activity;
DESCRIBE STREAM user_events;
DESCRIBE CONSUMER_GROUP analytics;

DROP TOPIC user_activity;
DROP STREAM user_events;
DROP CONSUMER_GROUP analytics;
```

Use `IF EXISTS` on drops.

See `scripts/test_streams_api.py`.
