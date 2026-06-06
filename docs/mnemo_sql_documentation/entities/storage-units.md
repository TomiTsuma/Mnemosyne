# Storage Units

Storage units abstract where table data lives (local paths, object storage, etc.).

## CREATE STORAGE UNIT

```sql
CREATE STORAGE_UNIT su_local TYPE LOCAL PATH '/data/mnemo/local';

CREATE STORAGE UNIT su_s3 TYPE S3
    BUCKET 'my-bucket'
    ENDPOINT 'https://s3.amazonaws.com'
    REGION 'us-east-1';
```

Syntax:

```ebnf
create_storage_unit ::= CREATE STORAGE_UNIT name TYPE type_name property*
```

Common properties (key followed by value):

| Property | Example |
|----------|---------|
| `PATH` | `PATH '/var/mnemo/data'` |
| `BUCKET` | `BUCKET 'logs'` |
| `ENDPOINT` | `ENDPOINT 'http://127.0.0.1:9000'` |
| `REGION` | `REGION 'eu-west-1'` |

Additional identifier keys are stored in a property map.

## Use with tables

```sql
CREATE TABLE file_backed (id Int64, payload String)
ENGINE = Memory
STORAGE_UNIT su_local;
```

## SHOW / DESCRIBE

```sql
SHOW STORAGE_UNITS;
SHOW STORAGE USAGE;
DESCRIBE STORAGE_UNIT su_local;
```

## DROP

```sql
DROP STORAGE_UNIT su_local;
DROP STORAGE UNIT IF EXISTS su_s3;
```

## Integration tests

See `scripts/test_storage_units_api.py` for LOCAL path setup and table binding.
