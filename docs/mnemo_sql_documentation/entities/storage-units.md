# Storage Units

`STORAGE_UNIT` is a named, cluster-wide storage resource. It abstracts **where** table data lives — local filesystem paths, S3-compatible object stores, and future backends — separately from the table **engine** (`Memory`, `File`, `MergeTree`).

## Purpose

- Centralize storage configuration (path, bucket, credentials)
- Bind tables to physical storage with `STORAGE_UNIT name` on `CREATE TABLE`
- Monitor capacity and usage across the cluster

## Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Unique storage unit identifier |
| `type` | enum | `LOCAL`, `S3` |
| `status` | enum | `ONLINE`, `DEGRADED`, `OFFLINE`, `MAINTENANCE` |
| `path` | string | Local filesystem path (`LOCAL` type) |
| `endpoint` | string | S3-compatible endpoint URL |
| `bucket` | string | Object store bucket name |
| `region` | string | Cloud region |
| `access_key` | string | S3 access key (stored; masked in `DESCRIBE`) |
| `secret_key` | string | S3 secret (stored; masked in `DESCRIBE`) |
| `capacity_bytes` | uint64 | Total capacity |
| `used_bytes` | uint64 | Bytes in use |
| `available_bytes` | uint64 | Free space |
| `owner` | string | Optional owner |
| `reference_count` | size_t | Tables bound to this unit |
| `created_at` | timestamp | Creation time |
| `updated_at` | timestamp | Last modification |

### Capabilities by type

| Type | Capabilities |
|------|--------------|
| `LOCAL` | `BLOCK_STORAGE`, `FILE_STORAGE` |
| `S3` | `OBJECT_STORAGE`, `REMOTE_READ`, `REMOTE_WRITE` |

## CREATE STORAGE_UNIT

```sql
-- Local disk
CREATE STORAGE_UNIT su_local TYPE LOCAL PATH '/data/mnemo/local';

-- S3-compatible object store
CREATE STORAGE_UNIT su_s3 TYPE S3
    BUCKET 'my-bucket'
    ENDPOINT 'https://s3.amazonaws.com'
    REGION 'us-east-1';

CREATE STORAGE_UNIT IF NOT EXISTS su_archive TYPE LOCAL PATH 'D:/mnemo/archive';
```

### Syntax (EBNF)

```ebnf
create_storage_unit ::= CREATE STORAGE_UNIT name TYPE type_name property*
property            ::= identifier string_or_number
```

Common property keys:

| Property | Required for | Example |
|----------|--------------|---------|
| `PATH` | `LOCAL` | `PATH '/var/mnemo/data'` |
| `BUCKET` | `S3` | `BUCKET 'logs'` |
| `ENDPOINT` | `S3` | `ENDPOINT 'http://127.0.0.1:9000'` |
| `REGION` | `S3` | `REGION 'eu-west-1'` |

Additional identifier keys are stored in the property map.

## Bind to a table

```sql
CREATE STORAGE_UNIT su_local TYPE LOCAL PATH '/tmp/mnemo_test';

CREATE TABLE file_backed (
    id      Float64,
    val     Float64,
    payload String
) ENGINE = File
  STORAGE_UNIT su_local;

INSERT INTO file_backed VALUES (1.0, 10.5, 'alpha'), (2.0, 20.5, 'beta');
SELECT * FROM file_backed ORDER BY id;
```

The `File` engine with a `STORAGE_UNIT` writes through the unit's disk backend.

## SHOW and DESCRIBE

```sql
SHOW STORAGE_UNITS;
SHOW STORAGE USAGE;
DESCRIBE STORAGE_UNIT su_local;
```

`DESCRIBE` returns fields including `name`, `type`, `status`, `path`/`bucket`, and `capabilities` (e.g. `block_storage`).

Example `SHOW STORAGE_UNITS` columns: `name`, `type`, `status`, `path`, `used_bytes`, `capacity_bytes`.

## DROP

```sql
DROP STORAGE_UNIT su_local;
DROP STORAGE UNIT IF EXISTS su_s3;
```

Dropping fails if `reference_count > 0` (tables still bound).

## Python example

```python
import tempfile
from pathlib import Path
from scripts._mnemo_client import Client

client = Client("http://127.0.0.1:1143")
path = Path(tempfile.mkdtemp()).as_posix()

client.query("CREATE DATABASE IF NOT EXISTS su_test_db")
client.query("USE su_test_db")
client.query(f"DROP STORAGE_UNIT IF EXISTS su_local")
client.query(f"CREATE STORAGE_UNIT su_local TYPE LOCAL PATH '{path}'")

r = client.query("SHOW STORAGE_UNITS")
assert "su_local" in r.body.lower()

r = client.query(f"DESCRIBE STORAGE_UNIT su_local")
assert "block_storage" in r.body.lower()

r = client.query(
    "CREATE TABLE su_file (id Float64, val Float64) "
    "ENGINE=File STORAGE_UNIT su_local"
)
assert r.ok()

r = client.query("INSERT INTO su_file VALUES (1.0, 10.5)")
r = client.query("SELECT * FROM su_file")
```

## Operational patterns

### Separate hot and cold storage

```sql
CREATE STORAGE_UNIT hot_ssd TYPE LOCAL PATH '/mnt/nvme/mnemo';
CREATE STORAGE_UNIT cold_s3 TYPE S3 BUCKET 'archive' ENDPOINT 'https://s3...' REGION 'us-east-1';

CREATE TABLE recent_events (...) ENGINE = File STORAGE_UNIT hot_ssd;
CREATE TABLE archived_events (...) ENGINE = File STORAGE_UNIT cold_s3;
```

### Monitor usage

```sql
SHOW STORAGE_USAGE;
-- Returns per-unit used/capacity for capacity planning dashboards
```

## Related

- [data-layer.md](data-layer.md) — table `STORAGE_UNIT` clause
- [nodes-and-clusters.md](nodes-and-clusters.md) — nodes with `TYPE STORAGE`
- `scripts/test_storage_units_api.py`
