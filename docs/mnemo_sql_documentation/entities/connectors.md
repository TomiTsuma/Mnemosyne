# Connectors

`CONNECTOR` integrates external systems — REST APIs, PostgreSQL, S3 — into the Mnemo catalog. Query remote data with `FROM CONNECTOR name.resource` syntax.

## Purpose

- Federated queries across internal tables and external sources
- Schema discovery for ETL pipeline design
- Health checks before pipeline runs

## Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Connector identifier |
| `type` | enum | `REST`, `POSTGRES`, `S3` |
| `status` | enum | `CREATING`, `VALIDATING`, `ACTIVE`, `DEGRADED`, `DISCONNECTED`, `DISABLED`, `ARCHIVED` |
| `auth` | enum | `ANONYMOUS`, `USERNAME_PASSWORD`, `API_KEY`, `OAUTH2`, `JWT`, `CERTIFICATE`, `IAM_ROLE` |
| `properties` | map | Type-specific config (`BASE_URL`, `HOST`, `BUCKET`, …) |
| `capabilities` | list | e.g. `READ`, `WRITE`, `SCHEMA_DISCOVERY` |
| `owner` | string | Optional owner |
| `last_test` | object | `ok`, `latency_ms`, `message`, `tested_at` |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last update |

Secret properties (`PASSWORD`, `SECRET_KEY`, `API_KEY`) are masked in `DESCRIBE`.

## CREATE CONNECTOR

### REST API

```sql
CREATE CONNECTOR api_rest TYPE REST
    BASE_URL 'http://127.0.0.1:8080'
    SCHEMA 'customers,orders';
```

`SCHEMA` lists discoverable resource names (mapped to URL paths like `/customers`).

### PostgreSQL

```sql
CREATE CONNECTOR api_pg TYPE POSTGRES
    HOST 'localhost'
    PORT 5432
    DATABASE 'sales'
    USER 'app'
    PASSWORD 'secret';
```

### S3

```sql
CREATE CONNECTOR api_s3 TYPE S3
    BUCKET 'data-lake'
    ENDPOINT 'http://127.0.0.1:9000'
    REGION 'us-east-1';
```

### Syntax

```ebnf
create_connector ::= CREATE CONNECTOR name TYPE type_name property*
```

Required: `TYPE` after connector name.

Common properties:

| Property | Types | Description |
|----------|-------|-------------|
| `BASE_URL` | REST | API root URL |
| `SCHEMA` | REST | Comma-separated resource list |
| `HOST`, `PORT`, `DATABASE` | POSTGRES | Connection target |
| `USER`, `PASSWORD` | POSTGRES | Credentials |
| `BUCKET`, `ENDPOINT`, `REGION` | S3 | Object store config |
| `AUTH` | All | Authentication method name |

## TEST CONNECTOR

```sql
TEST CONNECTOR api_rest;
```

Updates `last_test` with latency and success/failure message.

## DISCOVER SCHEMA

```sql
DISCOVER SCHEMA FROM CONNECTOR api_rest;
```

Returns resource names for use in `FROM CONNECTOR name.resource`.

## Query connector data

```sql
SELECT * FROM CONNECTOR api_rest.customers;
SELECT id, name FROM CONNECTOR api_rest.customers WHERE id = 1;
SELECT * FROM CONNECTOR api_rest.orders;
```

Join with local tables:

```sql
CREATE TABLE local_customers (id Int64, segment String) ENGINE = Memory;

SELECT c.name, l.segment
FROM CONNECTOR api_rest.customers c
JOIN local_customers l ON cast(c.id AS Int64) = l.id;
```

See [../queries/connector-queries.md](../queries/connector-queries.md).

## Load connector data into a table {#load-into-a-table}

A REST connector returns every value as text. Loading it into a typed table converts each value
to the column's type:

```sql
CREATE TABLE raw_orders (order_id Int64, customer_id Int64, total Float64);

INSERT INTO raw_orders
SELECT order_id, customer_id, total FROM CONNECTOR api_rest.orders;
```

The load is all-or-nothing: if one value does not fit its column (text that is not a number,
or a fractional value for an integer column) the statement fails and nothing is written. See
[../dml/README.md](../dml/README.md#insert-select).

## EXPORT TABLE {#export-table}

Writes a table, view or materialized view to a connector resource.

```sql
EXPORT TABLE orders TO CONNECTOR api_rest;                      -- resource "orders", APPEND
EXPORT TABLE orders TO CONNECTOR api_rest.archive;              -- resource "archive"
EXPORT TABLE orders TO CONNECTOR api_rest MODE OVERWRITE;
EXPORT TABLE big_orders TO CONNECTOR api_rest;                  -- a view
```

```ebnf
export ::= EXPORT TABLE table_name TO CONNECTOR connector_name [ '.' resource ]
           [ MODE ( APPEND | OVERWRITE ) ]
```

The result is one row: `connector`, `resource`, `mode`, `rows`.

| Mode | REST request | Effect |
|------|--------------|--------|
| `APPEND` (default) | `POST <BASE_URL>/<resource>` | Adds the rows to what is there |
| `OVERWRITE` | `PUT <BASE_URL>/<resource>` | Replaces what is there |

- The body is a JSON array with one object per row, sent in a single request, with the
  connector's API key header if it has one. A response other than 2xx is an error that
  includes the status.
- `OVERWRITE` changes data in the external system, which is why `APPEND` is the default.
  Appending zero rows sends nothing; overwriting with zero rows sends `[]`.
- A materialized view exports what it currently holds, not a fresh run of its query: refresh it
  first if you need current data.
- `MODE MERGE` and `MODE UPSERT` are recognised but not supported yet and fail with an error.
- Only **REST** connectors can be exported to. `POSTGRES` and `S3` connectors are catalog-only
  today, so exporting to them fails with a "not implemented" error, just as reading from them
  does.

## ALTER CONNECTOR

```sql
ALTER CONNECTOR api_rest SET BASE_URL 'http://10.0.0.1:8080';
ALTER CONNECTOR api_pg SET HOST 'db.internal';
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

## Python example (with mock REST server)

```python
import json
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from scripts._mnemo_client import Client

CUSTOMERS = json.dumps([
    {"id": "1", "name": "Alice", "age": "25"},
    {"id": "2", "name": "Bob", "age": "30"},
])

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/customers":
            body = CUSTOMERS.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(body)

httpd = HTTPServer(("127.0.0.1", 0), Handler)
port = httpd.server_address[1]
threading.Thread(target=httpd.serve_forever, daemon=True).start()

client = Client("http://127.0.0.1:1143")
base = f"http://127.0.0.1:{port}"

client.query("DROP CONNECTOR IF EXISTS api_rest")
client.query(
    f"CREATE CONNECTOR api_rest TYPE REST "
    f"BASE_URL '{base}' SCHEMA 'customers'"
)

r = client.query("TEST CONNECTOR api_rest")
assert r.ok()

r = client.query("DISCOVER SCHEMA FROM CONNECTOR api_rest")
assert "customers" in r.body.lower()

r = client.query("SELECT * FROM CONNECTOR api_rest.customers")
assert "Alice" in r.body
```

## ETL pipeline pattern

```sql
CREATE CONNECTOR crm TYPE REST BASE_URL 'https://crm/api' SCHEMA 'contacts';

CREATE PIPELINE crm_sync OWNER 'data-team';
CREATE STAGE load IN PIPELINE crm_sync ORDER 1;
CREATE TASK pull_contacts IN STAGE load IN PIPELINE crm_sync
    TYPE SQL
    BODY 'CREATE TABLE IF NOT EXISTS contacts (id Int64, name String) ENGINE=Memory';

RUN PIPELINE crm_sync;
```

## Related

- [pipelines.md](pipelines.md) — scheduled connector loads
- `scripts/test_connectors_api.py`, `scripts/test_export_api.py`
