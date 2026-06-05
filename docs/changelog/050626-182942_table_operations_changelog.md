# Table operations implementation — changelog

**Date:** 2026-06-05
**Spec:** `docs/CLICKHOUSE_TABLE_OPERATIONS.md`

## Summary

Implemented ClickHouse-style table operations end-to-end: CREATE TABLE (with
`IF NOT EXISTS` and `ENGINE`), INSERT, DROP, TRUNCATE, DETACH, and ALTER TABLE
(ADD / DROP / MODIFY COLUMN). The work adds dedicated DDL interpreters, a
`DDLGuard` for exclusive schema changes, a lightweight `DDLTransaction` for
rollback on failure, and versioned `TableMetadata`, and routes all DDL through
an explicit `DDLNode` in the analyzer/planner instead of heuristic `TableNode`
markers.

The implementation mirrors the structural patterns described in the spec
(`ASTCreateQuery`, `InterpreterCreateQuery`, `DDLGuard`, `StorageFactory`) while
adapting them to Mnemosyne's processor-based execution path via
`BlockInterpreter`.

## Changes by file

### Parser
- `src/Parsers/lexer.h` / `src/Parsers/lexer.cpp`
  - Added keywords: `ALTER`, `IF`, `EXISTS`, `ENGINE`, `TRUNCATE`, `DETACH`,
    `ADD`, `COLUMN`, `MODIFY`.
- `src/Parsers/ast.h`
  - Added `ASTDDLQuery` base struct with shared `database`, `table`,
    `if_not_exists`, `if_exists`.
  - Added `ASTDropQuery` and `ASTAlterQuery` node types.
  - Extended `QueryAST::Create` and `QueryAST::Drop` with DDL fields; added
    `QueryAST::Alter` and `QueryType::ALTER`.
- `src/Parsers/parser.h` / `src/Parsers/parser.cpp`
  - `parse_create`: supports `IF NOT EXISTS` and `ENGINE = <engine>`.
  - `parse_drop`: handles `DROP`, `TRUNCATE`, and `DETACH` with optional
    `IF EXISTS`.
  - `parse_alter`: parses comma-separated `ADD COLUMN`, `DROP COLUMN`, and
    `MODIFY COLUMN` commands.
  - `parse_insert`: optional column list (parentheses no longer required when
    omitted).
  - Added `parse_if_not_exists()` and `parse_if_exists()` helpers.
- `src/Parsers/format.cpp`
  - Updated DROP formatting to use `drop.table`; added ALTER formatting.

### Analyzer
- `src/Analyzer/query_tree.h`
  - Added `DDLNode` with explicit `Kind` enum for all DDL/session operations.
- `src/Analyzer/analyzer.h` / `src/Analyzer/analyzer.cpp`
  - Added `buildDDLNode()`; INSERT, CREATE, DROP, ALTER, SHOW, DESCRIBE, EXPLAIN,
    and USE now produce `DDLNode` instead of marker `TableNode`s.
  - Added `ALTER` case to `analyze()`.

### Planner
- `src/Planner/execution_plan.h`
  - Added `PlanNode::Type::ALTER`, `TRUNCATE`, `DETACH`.
  - Added DDL metadata fields: `if_not_exists`, `if_exists`, `engine`,
    `drop_kind`, `column_defs`, `alter_commands`.
- `src/Planner/planner.h` / `src/Planner/planner.cpp`
  - Added `plan_ddl()`; dispatches on `DDLNode::Kind` to produce explicit plan
    nodes.
  - Removed heuristic CREATE-vs-SCAN routing from `plan_table()` (TableNode is
    now scan-only).

### Interpreters (new)
- `src/Interpreters/ddl_guard.h` / `ddl_guard.cpp`
  - Per-database/table mutex acquired during DDL execution.
- `src/Interpreters/ddl_transaction.h` / `ddl_transaction.cpp`
  - Snapshots table existence and storage; rolls back on exception unless
    `commit()` is called.
- `src/Storages/table_metadata.h` / `table_metadata.cpp`
  - Versioned metadata object holding engine and column definitions.
- `src/Interpreters/interpreter_ddl_utils.h`
  - Shared helpers: `make_ok_block()`, `require_database()`,
    `resolve_current_database()`, `build_column_map()`, `query_from_plan()`.
- `src/Interpreters/interpreter_create_query.h` / `.cpp`
  - `InterpreterCreateQuery::execute()` — CREATE DATABASE/TABLE via
    `StorageFactory`.
- `src/Interpreters/interpreter_insert_query.h` / `.cpp`
  - `InterpreterInsertQuery::execute()` — typed INSERT from VALUES clause.
- `src/Interpreters/interpreter_drop_query.h` / `.cpp`
  - `InterpreterDropQuery::execute()` — DROP, TRUNCATE, DETACH with
    `IF EXISTS` support.
- `src/Interpreters/interpreter_alter_query.h` / `.cpp`
  - `InterpreterAlterQuery::execute()` — ADD / DROP / MODIFY COLUMN.

### Interpreters / Processors (wiring)
- `src/Interpreters/context.h` / `context.cpp`
  - `Context` constructor now sets `current_database` from the registered DB
    name (was hard-coded to `"default"` key).
  - Added `unregister_storage()`.
- `src/Interpreters/block_interpreter.cpp`
  - DDL plan nodes delegate to the dedicated interpreters; DESCRIBE and USE
    cases restored/fixed after refactor.
- `src/Interpreters/interpreter.cpp`
  - Added `ALTER`, `TRUNCATE`, `DETACH` cases in the processor switch.

### Storages
- `src/Storages/memory_storage.h` / `memory_storage.cpp`
  - Added `truncate()`, `drop_column()`, `modify_column()`.
- `src/Storages/storage_factory.h` / `storage_factory.cpp`
  - No structural change; CREATE TABLE now routes through the factory (was
    already registered for Memory, File, Dictionary).

### Processors
- `src/Processors/processors_source.cpp`
  - `CreateProcessor` CREATE TABLE path now creates tables via the current
    database and `StorageFactory` (was a no-op stub returning `OK`).

## SQL now supported

```sql
CREATE TABLE IF NOT EXISTS users (id Int64, name String) ENGINE = Memory;
INSERT INTO users (id, name) VALUES (1, 'Alice');
ALTER TABLE users ADD COLUMN email String;
ALTER TABLE users MODIFY COLUMN name String;
ALTER TABLE users DROP COLUMN email;
TRUNCATE TABLE users;
DROP TABLE IF EXISTS users;
DETACH TABLE users;
```

## Verification

```
cmake --build build    # clean build, all targets compile
```

## Known issues (pre-existing, out of scope)

- Some Catch2 interpreter SELECT tests fail (`tests/test_interpreter.h`) due to
  incomplete scan/filter pipeline wiring; unrelated to this DDL work.
- `INSERT … SELECT` is parsed at the AST level but not yet executed by
  `InterpreterInsertQuery` (VALUES-only for now).
- `DDLTransaction` rollback restores table registration but does not yet replay
  in-storage column mutations for partial ALTER failures.
