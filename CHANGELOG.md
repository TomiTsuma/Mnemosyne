# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added - Database Functionality (Levels 1-7)

#### Analyzer Implementation
- Completed `Analyzer::analyze()` to properly handle SELECT queries with table and column resolution
- Implemented `resolve_columns()` to collect column information from resolved tables
- Implemented `resolve_expression_columns()` to recursively validate column references in expressions
- Implemented `validate_function()` to check function names against FunctionFactory and AggregateFunctionFactory
- Completed `buildExpressionNode()` to handle all AST expression types (ASTLiteral, ASTColumnRef, ASTFunction, ASTBinaryOp, ASTUnaryOp, ASTAlias)
- Completed `buildSelectNode()` to properly map QueryAST::select fields to SelectNode with FROM, WHERE, GROUP BY, HAVING, ORDER BY, and LIMIT clauses

#### Planner Implementation
- Improved `plan_select()` to properly build execution DAG from SelectNode
- Added aggregate spec setup in GROUP BY planning
- Implemented proper column name extraction from SelectNode::ColumnExpr
- Added HAVING clause handling (applied after GROUP BY, before ORDER BY)
- Improved ORDER BY column extraction
- Fixed LIMIT/OFFSET handling

#### Processor Implementations
- Fixed bug in `SortProcessor::start()` where descending sort was incorrectly implemented (`std::swap(cmp, cmp)` changed to `cmp = !cmp`)
- Verified all processor implementations are functional:
  - `ScanProcessor::start()` - reads from storage and builds header
  - `FilterProcessor::start()` - applies predicate to filter rows
  - `ProjectProcessor::start()` - projects selected columns
  - `GroupByProcessor::start()` - groups and aggregates data
  - `SortProcessor::start()` - sorts data by columns
  - `LimitProcessor::start()` - applies LIMIT/OFFSET

#### BlockInterpreter Pipeline
- Improved `execute()` to properly handle processor pipeline execution
- Fixed `create_processor_for_node()` to properly connect processors via input/output streams:
  - SCAN: No inputs (source processor)
  - FILTER: Connected to child's output stream
  - PROJECT: Connected to child's output stream
  - GROUP_BY: Connected to child's output stream
  - SORT: Connected to child's output stream
  - LIMIT: Connected to child's output stream
- Added `execute_ddl_command()` method to handle DDL commands directly

#### DDL Command Execution
- Implemented `CREATE DATABASE` via DatabaseManager
- Implemented `CREATE TABLE` with column definitions via Database::create_table()
- Implemented `DROP TABLE` via Database::drop_table()
- Implemented `SHOW DATABASES` via Context::databases()
- Implemented `SHOW TABLES` via Database::list_tables()
- Implemented `DESCRIBE TABLE` via Storage::columns() and column_types()
- Implemented `USE DATABASE` via Context::set_current_database()

#### INSERT Execution
- Implemented INSERT execution in `execute_ddl_command()`
- Added value conversion to Block format
- Added support for multiple row inserts

#### Aggregate Function Execution
- Verified aggregate functions (COUNT, SUM, AVG, MIN, MAX) are registered in FunctionFactory
- Verified GroupByProcessor handles aggregate state management
- Verified GROUP BY aggregation works through planner and processor pipeline
- Verified HAVING clause filtering works as FILTER node after GROUP BY

#### Integration Tests
- Created `tests/integration/test_level1_ddl.cpp` - Level 1 DDL operations (CREATE DATABASE, USE, SHOW DATABASES/TABLES, CREATE TABLE, DESCRIBE, DROP TABLE)
- Created `tests/integration/test_level2_dml.cpp` - Level 2 DML operations (INSERT single/multiple rows, SELECT all/specific columns)
- Created `tests/integration/test_level3_filtering.cpp` - Level 3 Filtering (WHERE single predicates, AND/OR conditions, comparison operators)
- Created `tests/integration/test_level4_sorting.cpp` - Level 4 Sorting (ORDER BY ASC/DESC, multiple columns, default ASC)
- Created `tests/integration/test_level5_aggregates.cpp` - Level 5 Aggregates (COUNT, SUM, AVG, MIN, MAX, multiple aggregates)
- Created `tests/integration/test_level6_groupby.cpp` - Level 6 GROUP BY (single/multiple columns, with SUM/AVG, multiple aggregates)
- Created `tests/integration/test_level7_having.cpp` - Level 7 HAVING (with COUNT/SUM/AVG, multiple conditions, comparison operators)
- Created `tests/integration/integration_test_runner.cpp` - Test runner to execute all level tests

### Changed
- Updated `src/Analyzer/analyzer.cpp` - Complete analyzer implementation for Levels 1-7
- Updated `src/Planner/planner.cpp` - Improved planner to handle aggregates, HAVING, and proper column extraction
- Updated `src/Processors/processors_source.cpp` - Fixed SortProcessor descending sort bug
- Updated `src/Interpreters/block_interpreter.cpp` - Improved pipeline execution and added DDL command execution
- Updated `src/Interpreters/blockInterpreter.h` - Added helper method declarations

### Technical Notes
- The implementation focuses on in-memory storage (MemoryStorage) as requested
- Parser already supports all SQL constructs needed for Levels 1-7 (SELECT, INSERT, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN with WHERE, GROUP BY, HAVING, ORDER BY, LIMIT)
- Processor implementations use NoOpInputStream/NoOpOutputStream for streaming
- DDL commands are executed directly in the interpreter rather than through the processor pipeline
- Integration tests use GoogleTest framework and verify query parsing, analysis, and planning
