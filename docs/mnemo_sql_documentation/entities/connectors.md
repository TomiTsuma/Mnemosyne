# Connectors

Connectors integrate external systems (REST, PostgreSQL, S3, …) into Mnemo catalog and queries.

## CREATE CONNECTOR

```sql
CREATE CONNECTOR api_rest TYPE REST
    BASE_URL 'http://127.0.0.1:8080'
    SCHEMA 'customers,orders';

CREATE CONNECTOR api_pg TYPE POSTGRES
    HOST 'localhost'
    PORT 5432
    DATABASE 'sales'
    USER 'app'
    PASSWORD 'secret';

CREATE CONNECTOR api_s3 TYPE S3
    BUCKET 'data-lake'
    ENDPOINT 'http://127.0.0.1:9000'
    REGION 'us-east-1';
```

Required: `TYPE type_name` after connector name.

Optional properties: `AUTH`, `PATH`, `BUCKET`, `ENDPOINT`, `REGION`, `HOST`, `PORT`, `DATABASE`, `SCHEMA`, or custom `identifier value` pairs.

## TEST CONNECTOR

```sql
TEST CONNECTOR api_rest;
```

## DISCOVER SCHEMA

```sql
DISCOVER SCHEMA FROM CONNECTOR api_rest;
```

Returns discovered resource names (tables/endpoints) for use in `FROM CONNECTOR name.resource`.

## Query connector data

```sql
SELECT * FROM CONNECTOR api_rest.customers;
```

See [queries/connector-queries.md](../queries/connector-queries.md).

## ALTER CONNECTOR

```sql
ALTER CONNECTOR api_rest SET BASE_URL 'http://10.0.0.1:8080';
```

## SHOW

```sql
SHOW CONNECTORS;
SHOW CONNECTOR CAPABILITIES api_rest;
SHOW CONNECTOR STATUS api_rest;
```

## DESCRIBE / DROP

```sql
DESCRIBE CONNECTOR api_rest;
DROP CONNECTOR api_rest;
DROP CONNECTOR IF EXISTS api_rest;
```

See `scripts/test_connectors_api.py`.
