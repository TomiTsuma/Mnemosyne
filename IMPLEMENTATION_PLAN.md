# Mnemosyne Implementation Plan

## Current setup status

- Repository configured successfully with CMake 4.3.3.
- Build files were generated into `build/` with `-DENABLE_TESTS=ON`.
- The test target `mnemosyne_tests` is being built to validate core compilation.
- `TEST_QUERIES.md` defines 13 levels of SQL and OLAP functionality.

## Key objectives

1. Foundation
   - Complete SQL parser for DDL/DML/SELECT and expression grammar.
   - Complete semantic analyzer and query tree builder.
   - Complete planner to emit execution plan DAG.

2. Execution engine
   - Implement processor pipeline stages: Scan, Filter, Project, GroupBy, Sort, Limit.
   - Implement joins and basic aggregation functions.
   - Implement `BlockInterpreter` execution.

3. SQL feature support
   - DDL: CREATE DATABASE, USE, SHOW, CREATE TABLE, DESCRIBE, DROP TABLE.
   - DML: INSERT INTO ... VALUES.
   - Queries: SELECT, WHERE, ORDER BY, GROUP BY, HAVING, LIMIT.
   - Joins: INNER, LEFT, multiple joins.
   - Subqueries: scalar, IN, uncorrelated.
   - Window functions: SUM() OVER(...), RANK().

4. Columnar database features
   - Column pruning.
   - Predicate pushdown.
   - Late materialization.
   - Stress tests for large scan, aggregation, sort, join + group by.

5. Testing
   - Unit tests for parser, analyzer, planner, processors.
   - Integration tests covering each `TEST_QUERIES.md` level.
   - HTTP `/query` endpoint end-to-end validation.

## Immediate next steps

- Wait for the `mnemosyne_tests` build completion and inspect compiler output.
- Confirm which subsystems are already implemented versus stubbed.
- Begin with parser completion and test-driven development for Level 1 and Level 2.
- Add file-level tasks in this document as implementation progresses.

## Target milestone

- Have a working execution path from SQL text through parser, analyzer, planner, interpreter, and processor pipeline.
- Pass Level 1 and Level 2 tests first, then expand into filtering, grouping, joins, and analytics.
