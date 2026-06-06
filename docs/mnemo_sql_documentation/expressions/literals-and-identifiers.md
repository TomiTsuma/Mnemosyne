# Literals and Identifiers

## Numeric literals

```sql
0
42
-17
3.14159
.5
1e10   -- if supported by lexer number reader
```

Integers parse to Int64 in the AST; floats to Float64.

## String literals

Single-quoted only in the lexer:

```sql
'hello'
'O''Reilly'    -- escaped quote
''             -- empty string
```

## Boolean literals

```sql
TRUE
FALSE
```

## NULL

```sql
NULL
```

Represents SQL null in literals; column nullability is schema-defined.

## Identifiers

```sql
customer_id
_orders
t1
```

Reserved words used as names may require quoting when quoting is implemented; prefer non-reserved names.

## Qualified names

```sql
orders.amount
o.amount   -- with alias o
```

## Star

```sql
SELECT * FROM t;
COUNT(*)   -- inside aggregate
```

## Function names as identifiers

`SUM`, `COUNT`, `AVG`, `MIN`, `MAX` are tokenized as keywords but accepted as aggregate function names in the select list.

## Table names in DDL/DML

```sql
INSERT INTO my_table ...
CREATE TABLE my_table ...
FROM my_table
```

Database qualification: `db.table` when the interpreter resolves cross-database access.
