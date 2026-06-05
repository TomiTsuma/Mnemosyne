# USE statement implementation — changelog

**Date:** 2026-06-05
**Branch:** `feature/use-statement`
**Spec:** `docs/USE_IMPLEMENTATION.md`

## Summary

Implemented the SQL `USE [DATABASE] <name>` statement end-to-end, so a session
can set its *current database* and have subsequent queries resolve against it.
The statement now flows through the entire pipeline: lexer → parser → analyzer →
planner → interpreter → processor, and is reachable through the HTTP `/query`
API. The server keeps a single long-lived `Context`, so `USE` persists across
HTTP requests.

The implementation mirrors ClickHouse's `ASTUseQuery` / `ParserUseQuery` /
`InterpreterUseQuery`, adapted to Mnemosyne's processor-based architecture and
following the existing `CREATE DATABASE` pattern.

## Changes by file

### Parser
- `src/Parsers/ast.h`
  - Added `USE` to `QueryAST::QueryType`.
  - Added a `Use { std::string database_name; } use;` struct to `QueryAST`.
- `src/Parsers/parser.h`
  - Changed the `parse_use` declaration to match the other statement parsers:
    `auto parse_use(std::unique_ptr<QueryAST>& ast) -> void`.
- `src/Parsers/parser.cpp`
  - Dispatched `KeywordUse` to `parse_use` in `parse_query`.
  - Implemented `parse_use`, accepting both `USE <name>` and the optional
    `USE DATABASE <name>` keyword form (matching ClickHouse).
  - **Bug fix:** `parse_create` wrote to `.database_name` instead of
    `create.database_name`, which would not compile. Corrected.

### Analyzer
- `src/Analyzer/analyzer.cpp`
  - Added a `USE` case to `analyze` (no semantic resolution required, like
    `CREATE DATABASE`).
  - Added a `USE` case to `buildQueryTree`, encoding the target database in a
    `TableNode` marked `use_database` (database field carries the target name).

### Planner
- `src/Planner/execution_plan.h`
  - Added `USE` to `PlanNode::Type`.
- `src/Planner/planner.cpp`
  - Recognised the `use_database` `TableNode` marker and produced a
    `PlanNode` of type `USE` with the target database name in `name`. The branch
    is placed before the generic table branches so it cannot be mis-routed.

### Interpreters / Processors
- `src/Interpreters/block_interpreter.cpp` (the path used by the HTTP server)
  - Routed `PlanNode::Type::USE` through the direct DDL execution path.
  - Implemented the `USE` case in `execute_ddl_command`: it validates the target
    database exists and calls `set_current_database`. If the database is unknown
    it throws an exception rather than silently switching context (no-fallback /
    fail-loud principle).
- `src/Processors/processors.h` and `src/Processors/processors_source.cpp`
  - Added `UseProcessor`, used by the alternative `Interpreter::execute` path.
    It performs the same existence check and sets the current database, returning
    a single-cell `OK` acknowledgement block.
- `src/Interpreters/interpreter.cpp`
  - Added the `PlanNode::Type::USE` case constructing a `UseProcessor`.

### Server
- `src/Server/http_handler.cpp`
  - Exposed the session's current database via the `/metrics` JSON endpoint
    (`"current_database": "<name>"`). This is the observable used by the API
    test. The `/status` endpoint was intentionally left untouched (see Known
    issues).

### Documentation
- `docs/GRAMMAR.md`
  - Added `<use_statement> → USE [DATABASE] <name>`, listed `USE` among reserved
    keywords, and added usage examples.

### Tests
- `scripts/test_use_api.py`
  - New stdlib-only (no external dependencies) Python script that launches
    `mnemosyne_server`, waits for `/ping`, and exercises the HTTP API:
    connectivity, `CREATE DATABASE`, `USE <db>`, `USE DATABASE <db>`, session
    persistence across requests, and the error path for `USE <missing db>`.
  - Run: `python scripts/test_use_api.py` (exits non-zero on failure).
  - Result at time of writing: **15/15 checks passed**.

## Verification

```
cmake --build build --target mnemosyne_server --config Release   # clean, no warnings
python scripts/test_use_api.py                                   # 15/15 checks passed
```

## Known issues (pre-existing, out of scope)

- The HTTP `/status` endpoint segfaults because the server constructs its
  `Context` with `Context(nullptr)`, which never initialises the internal
  `pool_` (`std::unique_ptr<ThreadPool>`); `handle_status` then dereferences the
  null pool via `context_.pool().active_count()`. This is unrelated to `USE` and
  predates this change. The current-database observable was therefore added to
  `/metrics` (which only touches the safe `total_memory`) instead of `/status`.
