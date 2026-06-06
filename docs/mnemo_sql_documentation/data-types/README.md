# Data Types

Mnemo column types are defined at table creation and resolved through the type system in `src/DataTypes/`. Type names in SQL generally follow ClickHouse-style naming.

## Primitive types

| SQL name | Description | Notes |
|----------|-------------|-------|
| `UInt8` | 8-bit unsigned integer | 0 … 255 |
| `UInt16` | 16-bit unsigned | |
| `UInt32` | 32-bit unsigned | |
| `UInt64` | 64-bit unsigned | Common for IDs |
| `Int8` | 8-bit signed | |
| `Int16` | 16-bit signed | |
| `Int32` | 32-bit signed | |
| `Int64` | 64-bit signed | Common default for integers |
| `Float32` | 32-bit IEEE float | |
| `Float64` | 64-bit IEEE float | |
| `String` | Variable-length UTF-8 string | |
| `FixedString(N)` | Fixed-width string | N = length |
| `Date` | Calendar date | Literal: `'2025-01-15'` |
| `DateTime` | Timestamp | Literal: `'2025-01-15 12:00:00'` |
| `Bool` | Boolean | `TRUE` / `FALSE` |
| `UUID` | 128-bit UUID | |
| `Decimal` | Decimal fixed-point | Precision/scale as implemented |
| `Array(T)` | Nested array type | Composite; see factory |

## Aliases and documentation names

Older docs may refer to `VARCHAR`, `TIMESTAMP`, or `INT64`. In DDL, prefer the native names above (`String`, `DateTime`, `Int64`). The parser accepts arbitrary identifier tokens as type names; unknown types fail at analysis or execution.

## Type compatibility

Numeric types are mutually compatible for many operations (see `types_are_compatible` in `data_type.cpp`). Exact behavior depends on the analyzer and function implementations.

## Table definition example

```sql
CREATE TABLE metrics (
    event_id   UInt64,
    user_id    Int64,
    score      Float64,
    tag        String,
    created_at DateTime
) ENGINE = Memory;
```

## Literals vs types

| Literal | Typical inferred type |
|---------|----------------------|
| `42` | Integer (Int64 in parser AST) |
| `3.14` | Float64 |
| `'text'` | String |
| `NULL` | Nullable context only |
| `TRUE` / `FALSE` | Bool |

Insert statements pass values as strings in the AST; the server coerces to column types.

## Engine and storage

`ENGINE = Memory` is the common default for tests. MergeTree-family engines are planned for production storage (see project README). Engine name is a bare identifier after `ENGINE =`.

## Composite and specialized types

- **LowCardinality(String)** — referenced in README examples; dictionary encoding when supported by storage.
- **Array** — nested values; syntax `Array(Int64)` when composite parsing is enabled.

Refer to `src/DataTypes/data_type_factory.cpp` for the authoritative list of registered types.
