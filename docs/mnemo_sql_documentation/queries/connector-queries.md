# Connector Queries

Connectors expose external systems (REST, PostgreSQL, S3, etc.) as queryable resources.

## Prerequisites

1. Create a connector (see [entities/connectors.md](../entities/connectors.md))
2. Optionally discover schema: `DISCOVER SCHEMA FROM CONNECTOR name`
3. Query with `FROM CONNECTOR name.resource`

## SELECT from connector

```sql
-- All columns from REST resource "customers"
SELECT * FROM CONNECTOR api_rest.customers;

-- Projection and filter
SELECT order_id, total
FROM CONNECTOR api_rest.orders
WHERE total > 20;
```

Syntax:

```ebnf
from_connector ::= CONNECTOR identifier '.' identifier
```

The first identifier is the connector name; the second is the resource/table name returned by discovery.

## Joining connector data

Pattern for enriching local keys with connector attributes:

```sql
SELECT l.id, c.name
FROM local_customers l
INNER JOIN /* connector join pattern */ ...
```

For Phase 1, typical tests query connector resources directly or load into local tables via pipelines.

## Capabilities and status

```sql
SHOW CONNECTOR CAPABILITIES api_rest;
SHOW CONNECTOR STATUS api_rest;
TEST CONNECTOR api_rest;
```

## Error cases

- Unknown connector — execution error
- Unknown resource — connector-specific error
- Missing credentials — `TEST CONNECTOR` reports failure

See `scripts/test_connectors_api.py` for end-to-end examples.
