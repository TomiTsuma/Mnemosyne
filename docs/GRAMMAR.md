# Mnemosyne SQL Grammar Reference

Mnemosyne supports a subset of SQL with extensions for analytical workloads.

## Full Grammar (EBNF)

```
<query>           → <statement> ";"
<statement>       → <select_statement> | <insert_statement> | <update_statement>
                   | <delete_statement> | <create_database_statement>
                   | <create_table_statement> | <drop_table_statement>
                   | <alter_table_statement> | <drop_database_statement>
                   | <use_statement> | <set_statement> | <explain_statement>

<select_statement> → SELECT [ALL | DISTINCT] <select_list>
                    [FROM <table_name>]
                    [WHERE <condition>]
                    [GROUP BY <column> (, <column>)*]
                    [HAVING <condition>]
                    [ORDER BY <column> [ASC | DESC]]
                    [LIMIT <number>]

<select_list>     → <select_item> (, <select_item>)*
<select_item>     → <expression> [AS <alias>] | *

<insert_statement> → INSERT INTO <table_name> <column_list> VALUES <value_list>
<column_list>     → ( <column> (, <column>)* )
<value_list>      → ( <value> (, <value>)* )

<update_statement> → UPDATE <table_name> SET <assignment_list> [WHERE <condition>]
<assignment_list> → <assignment> (, <assignment>)*
<assignment>      → <column> = <expression>

<delete_statement> → DELETE FROM <table_name> [WHERE <condition>]

<create_database_statement> → CREATE DATABASE <name>
<drop_database_statement>   → DROP DATABASE <name>

<create_table_statement> → CREATE TABLE <table_name>
                           ( <column_def> (, <column_def>)* )
<column_def>           → <column_name> <data_type> [NOT NULL]
<data_type>            → INT64 | FLOAT64 | VARCHAR | DATE | TIMESTAMP

<drop_table_statement>   → DROP TABLE <table_name>
<alter_table_statement>  → ALTER TABLE <table_name> ADD <column_def>
                           | ALTER TABLE <table_name> DROP <column>

<use_statement>     → USE [DATABASE] <name>
<set_statement>     → SET <name> = <value>
<explain_statement> → EXPLAIN <select_statement>

<expression>    → <additive_expression>
<additive_expression> → <multiplicative_expression> ( "+" | "-" ) <multiplicative_expression>
<multiplicative_expression> → <unary_expression> ( "*" | "/" | "%" ) <unary_expression>
<unary_expression> → "-" <primary_expression> | <primary_expression>
<primary_expression> → <literal> | <column_ref> | <parenthesized_expression>
                          | <function_call>
<function_call>  → <function_name> ( <argument_list> )
<function_name>  → "add" | "sub" | "mul" | "div" | "sum" | "count" | "avg"
                  | "min" | "max" | "eq" | "ne" | "gt" | "lt" | "ge" | "le"

<literal>       → <integer_literal> | <float_literal> | <string_literal>
<column_ref>    → <identifier> ( "." <identifier> )?
<identifier>    → [a-zA-Z_][a-zA-Z0-9_]*

<condition>     → <expression> <comparison_operator> <expression>
<comparison_operator> → "=" | "!=" | ">" | "<" | ">=" | "<=" | "LIKE"

<value>         → <literal> | <expression>
```

## Reserved Keywords

- SELECT, FROM, WHERE, GROUP, BY, HAVING, ORDER, LIMIT, ALL, DISTINCT
- INSERT, INTO, VALUES, UPDATE, SET, DELETE
- CREATE, DATABASE, TABLE, DROP, ALTER, ADD, COLUMN, USE
- IF, EXISTS, NOT, NULL, AND, OR, IN, BETWEEN, CASE, WHEN, THEN, ELSE, END
- ASC, DESC
- JOIN, ON, LEFT, RIGHT, INNER, OUTER
- AS, EXPLAIN

## Data Types

| Type | Description | Example |
|------|-------------|---------|
| INT8 | 8-bit signed integer | 127 |
| INT16 | 16-bit signed integer | 32767 |
| INT32 | 32-bit signed integer | 2147483647 |
| INT64 | 64-bit signed integer | 9223372036854775807 |
| UINT8 | 8-bit unsigned integer | 255 |
| UINT16 | 16-bit unsigned integer | 65535 |
| UINT32 | 32-bit unsigned integer | 4294967295 |
| UINT64 | 64-bit unsigned integer | 18446744073709551615 |
| FLOAT32 | 32-bit floating point | 3.14f |
| FLOAT64 | 64-bit floating point | 3.14 |
| VARCHAR | Variable-length string | 'Hello' |
| DATE | Date | '2024-01-01' |
| TIMESTAMP | Timestamp | '2024-01-01 12:00:00' |

## Aggregate Functions

| Function | Description | Example |
|----------|-------------|---------|
| SUM(col) | Sum of values | SUM(salary) |
| COUNT(col) | Count of values | COUNT(*) |
| AVG(col) | Average of values | AVG(salary) |
| MIN(col) | Minimum value | MIN(salary) |
| MAX(col) | Maximum value | MAX(salary) |

## Scalar Functions

| Function | Description | Example |
|----------|-------------|---------|
| add(a, b) | Addition | add(1, 2) |
| sub(a, b) | Subtraction | sub(2, 1) |
| mul(a, b) | Multiplication | mul(2, 3) |
| div(a, b) | Division | div(6, 2) |
| eq(a, b) | Equality | eq(1, 1) |
| ne(a, b) | Not equal | ne(1, 2) |
| gt(a, b) | Greater than | gt(2, 1) |
| lt(a, b) | Less than | lt(1, 2) |
| ge(a, b) | Greater or equal | ge(2, 2) |
| le(a, b) | Less or equal | le(1, 2) |

## Examples

```sql
SELECT name, SUM(amount) FROM orders GROUP BY name ORDER BY SUM(amount) DESC;

INSERT INTO orders (id, name, amount) VALUES (1, 'Alice', 100.0);

UPDATE orders SET amount = 200.0 WHERE name = 'Alice';

DELETE FROM orders WHERE amount < 10.0;

CREATE TABLE users (id INT64 NOT NULL, name VARCHAR);

USE analytics;          -- set the session's current database
USE DATABASE analytics; -- equivalent, with the optional DATABASE keyword

EXPLAIN SELECT * FROM orders WHERE amount > 100.0;
```
