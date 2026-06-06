# Expressions

Mnemo SQL expressions combine literals, column references, operators, and function calls. The parser uses recursive descent with the following precedence (lowest to highest):

1. `OR`
2. `AND`
3. Comparison (`=`, `!=`, `<>`, `<`, `>`, `<=`, `>=`, `IN`)
4. Additive (`+`, `-`)
5. Multiplicative (`*`, `/`, `%`)
6. Unary (`-`, `NOT`) — limited in current parser
7. Primary (literals, identifiers, `( expr )`, functions, subqueries)

## Subsections

- [operators.md](operators.md)
- [literals-and-identifiers.md](literals-and-identifiers.md)
- [functions.md](functions.md)

## Boolean expressions

Used in `WHERE`, `HAVING`, and `ON`:

```sql
WHERE amount > 100 AND region = 'US-East'
WHERE status IN (SELECT status FROM allowed)
WHERE id IN (1, 2, 3)   -- list form in grammar stub; subquery form parsed
```

## Arithmetic

```sql
SELECT price * quantity AS line_total FROM lines;
SELECT (a + b) / 2 AS mid FROM t;
```

## Column references

```sql
column_name
table.column
alias.column
*
```

## Aliases in select list

```sql
SELECT amount * 2 AS doubled FROM t;
```

`AS alias` is parsed at the term level after the expression.

## Planned expression features (grammar stub)

Not fully implemented in `parse_comparison` / `parse_factor`:

- `BETWEEN a AND b`
- `CASE WHEN … THEN … END`
- `CAST(expr AS type)`
- `expr IS [ NOT ] NULL`
- `expr LIKE pattern`
- Array literals `ARRAY [1, 2, 3]`

Verify support before use.
