# Data Manipulation (DML)

Mnemo SQL currently supports **INSERT** for loading data. `UPDATE` and `DELETE` are not implemented in the parser.

## INSERT syntax

```ebnf
insert_statement ::= INSERT INTO table_name [ column_list ] VALUES value_lists

column_list   ::= '(' identifier ( ',' identifier )* ')'
value_lists   ::= '(' value ( ',' value )* ')'
                  ( ',' '(' value ( ',' value )* ')' )*
```

## Insert with implicit columns

All columns in table order:

```sql
INSERT INTO customers VALUES
    (1, 'Alice', 25),
    (2, 'Bob', 30);
```

## Insert with column list

```sql
INSERT INTO customers (customer_id, name, age) VALUES
    (3, 'Charlie', 35);
```

## Single row

```sql
INSERT INTO orders VALUES (1, 'US-East', 99.50);
```

## Types in VALUES

Values are parsed as expressions (literals). Strings use single quotes; numbers are integer or float tokens:

```sql
INSERT INTO events VALUES
    (1, '2025-01-15 10:00:00', 'payload text'),
    (2, '2025-01-16 11:00:00', 'other');
```

The server coerces string forms to column types (`DateTime`, `Int64`, etc.).

## INSERT … SELECT

The grammar stub supports `INSERT INTO table SELECT …`. Verify interpreter support before use in production; the primary test path is `INSERT … VALUES`.

## Truncate

Remove all rows while keeping table definition:

```sql
TRUNCATE TABLE orders;
```

`TRUNCATE` is parsed as a drop-kind variant (see [drop-and-truncate](../ddl/drop-and-truncate.md)).

## Future DML

`UPDATE` and `DELETE` are documented in legacy `docs/GRAMMAR.md` but are **not** present in `Parser::parse_query()`. Track parser changes before adopting.
