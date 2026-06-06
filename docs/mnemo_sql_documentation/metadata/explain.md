# EXPLAIN

Returns the execution plan for a nested query statement.

## Syntax

```sql
EXPLAIN select_statement;
EXPLAIN INSERT INTO t VALUES (1);
EXPLAIN CREATE TABLE t (id Int64);
```

The parser recursively parses the statement following `EXPLAIN` and attaches it to the explain AST node.

## Example

```sql
EXPLAIN SELECT region, sum(amount)
FROM orders
WHERE amount > 10
GROUP BY region;
```

Use this to debug analyzer and planner choices (filter pushdown, aggregation strategy, storage reads).

## Limitations

- Plan format and detail level depend on interpreter implementation.
- Not all entity control statements may produce meaningful plans.

## Related

An `EXPLAIN` action also exists in the model control AST for model-specific explanations (future/parallel path to SQL `EXPLAIN`).
