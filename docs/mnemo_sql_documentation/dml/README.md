# Data Manipulation (DML)

Mnemo SQL currently supports **INSERT** for loading data. `UPDATE` and `DELETE` are not implemented in the parser.

## INSERT syntax

```ebnf
insert_statement ::= INSERT INTO table_name [ column_list ] ( VALUES value_lists | select )

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

## INSERT … SELECT {#insert-select}

Load the result of a query into a table:

```ebnf
insert_select ::= INSERT INTO table_name [ column_list ] select
```

```sql
INSERT INTO dst SELECT id, name, score FROM src;
INSERT INTO dst SELECT * FROM src WHERE id > 1;
INSERT INTO totals SELECT name, COUNT(*), SUM(score) * 2 FROM src GROUP BY name;
INSERT INTO raw_orders SELECT order_id, customer_id, total FROM CONNECTOR api_rest.orders;
```

- Rows are **appended**; existing rows stay.
- The query's columns are matched to the target's columns by position. Without a column list
  the query must return exactly the table's columns; with one, the listed columns are filled
  from the query and the rest get their default value.
  ```sql
  INSERT INTO dst (id, name) SELECT id + 10, name FROM src WHERE id = 1;   -- score defaults to 0
  ```
- Each value is converted to its target column's type. Text must be a whole number or a number
  to go into a numeric column, and a fractional value does not fit an integer column; either
  fails the statement. This is what makes loading from a connector, which returns text, work.
- **All-or-nothing:** every value is converted before anything is written, so one bad value
  leaves the table unchanged.
- A table can feed itself (`INSERT INTO src SELECT … FROM src` doubles it).
- A column-count mismatch, an unknown target table, or an unknown column fails with an error
  naming it. The target must be a table; inserting into a stream this way is not supported.

See [../entities/connectors.md](../entities/connectors.md#load-into-a-table) and
[../entities/pipelines.md](../entities/pipelines.md) for loading in a pipeline.

## Truncate

Remove all rows while keeping table definition:

```sql
TRUNCATE TABLE orders;
```

`TRUNCATE` is parsed as a drop-kind variant (see [drop-and-truncate](../ddl/drop-and-truncate.md)).

## Future DML

`UPDATE` and `DELETE` are documented in legacy `docs/GRAMMAR.md` but are **not** present in `Parser::parse_query()`. Track parser changes before adopting.
