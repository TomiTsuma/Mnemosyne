# Operators

## Comparison

| Operator | Meaning |
|----------|---------|
| `=` | Equal |
| `!=`, `<>` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |
| `IN ( subquery )` | Membership (subquery form parsed) |

## Logical

| Operator | Meaning |
|----------|---------|
| `AND` | Logical and |
| `OR` | Logical or |
| `NOT` | Logical not (unary; partial support) |

## Arithmetic

| Operator | Meaning |
|----------|---------|
| `+` | Addition |
| `-` | Subtraction (binary) or negation (unary) |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo (integer operands give an integer; a zero divisor raises an error) |

`+`, `-`, `*` and `%` on two integers give an integer; any fractional operand gives a floating-point result. `/` always gives a floating-point result, so `7 / 2` is `3.5`.

## String concatenation

The lexer recognizes `->` as a concatenation token (`TokenType::Concat`) for future string ops.

## Operator functions

Comparison and arithmetic can also be expressed as functions (see [functions.md](functions.md)):

```sql
SELECT eq(a, b), gt(a, b), add(a, b) FROM t;
```

## Precedence {#precedence}

From loosest to tightest binding: `OR`, `AND`, comparisons (`=`, `!=`, `<`, `>`, `<=`, `>=`, `IN`), `+ -`, `* / %`, unary `-`.

## Precedence examples

```sql
-- AND binds tighter than OR
WHERE a = 1 OR b = 2 AND c = 3
-- means: a = 1 OR (b = 2 AND c = 3)

-- Use parentheses for clarity
WHERE (a = 1 OR b = 2) AND c = 3
```

```sql
-- Multiplication before addition
SELECT 1 + 2 * 3;   -- 7
SELECT (1 + 2) * 3; -- 9
```
