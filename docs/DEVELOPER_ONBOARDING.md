# Developer Onboarding Guide: Mnemosyne

> **Last updated:** 2026-06-02

---

## What You're Getting Into

Mnemosyne is a **column-oriented analytical DBMS** (OLAP database) written in C++23, inspired by ClickHouse's architecture. Think of it as a database engine that processes data column-by-column instead of row-by-row — this is what gives it massive compression ratios and vectorized scan speeds.

**Current state:** The project has a **solid architectural foundation** (~60% of the design is in place) but **most functional implementation is still stubbed** (~80%). The parser, type system, column storage, and storage engines are implemented; the actual query execution pipeline, JOINs, and distributed features are work-in-progress.

**Prior knowledge that helps:** Understanding of database internals (parsing, query planning, storage engines), modern C++ (smart pointers, templates, std::expected), and the ClickHouse architecture model will make this codebase much easier to navigate.

---

## Before You Start

**Prerequisites:**

- **C++23 compiler** — MSVC 19.40+ (VS 2022 17.8+), GCC 14+, or Clang 18+
- **CMake 3.28+** — required for C++23 standard enforcement
- **Ninja** (recommended) or Make
- **Git**

**Helpful background docs:**

- [Architecture Overview](ARCHITECTURE.md) — system architecture and data flow
- [API Reference](API.md) — public module interfaces
- [SQL Grammar](GRAMMAR.md) — supported SQL dialect
- [Technical Analysis](TECHNICAL_ANALYSIS.md) — deep dive into every subsystem

---

## First Run (Getting it Working)

### Windows / MSVC

```powershell
# Clone
git clone <repo>
cd Mnemosyne

# Configure
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run
cd ..\bin\Release
.\mnemosyne_server.exe
```

Expected output:
```
========================================
  Mnemosyne DBMS v0.1.0
  Column-oriented analytical database
========================================
[Server] Starting on port 1143 (HTTP)
[Server] Use http://localhost:1143 to connect
```

### WSL2 / Linux

```bash
# Install prerequisites
sudo apt update && sudo apt install -y build-essential cmake git

# Verify compiler
g++ --version   # 14+ required

# Build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run
./bin/Release/mnemosyne_server
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

---

## The Mental Model

Think of Mnemosyne as a **factory assembly line** with six stages:

```
Raw SQL (raw material)
    │
    ▼
┌─────────────────────┐
│ Stage 1: Parser     │  Tokenize → AST (like a compiler front-end)
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Stage 2: Analyzer   │  Validate types, resolve columns, build IR
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Stage 3: Planner    │  IR → execution DAG (the "compiler optimizer")
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Stage 4: Interpreter│  DAG → processor pipeline (the "code generator")
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Stage 5: Processors │  Execute pipeline: scan → filter → aggregate → sort
└──────────┬──────────┘
           │
           ▼
    Results (finished product)
```

**Key insight:** Data flows **column-by-column** through this pipeline. The `Block` is the fundamental unit of data — a rectangular table where each column is stored contiguously.

### The Three Pillars

1. **Columnar data model** — `Block` → `Column<T>` → `Field` (single value). Data is laid out by column, not row.
2. **Pipeline execution** — queries are expressed as a DAG of processors (Scan → Filter → Project → Aggregate → Sort → Limit). Each processor reads from its upstream processor's output.
3. **Plugin architecture** — storages, databases, functions, codecs, and disk backends are all registered via singleton factories. New functionality = implement an interface + register it.

---

## Repository Layout (Where Things Live)

```
Mnemosyne/
├── CMakeLists.txt              ← Top-level build (C++23, CMake 3.28+)
├── programs/
│   ├── server/main.cpp         ← Server daemon entry point
│   └── client/main.cpp         ← CLI client entry point
├── src/                        ← Core source (14 subsystems)
│   ├── Core/                   ← Block, Column, Field, Series
│   ├── DataTypes/              ← IDataType hierarchy, type factory
│   ├── Columns/                ← ColumnVector<T>, ColumnString, ColumnArray
│   ├── Parsers/                ← Lexer, Parser, AST, grammar
│   ├── Analyzer/               ← Semantic analysis, passes, query tree
│   ├── Planner/                ← Execution DAG, cost model
│   ├── Functions/              ← Scalar function registry
│   ├── AggregateFunctions/     ← Aggregate function registry
│   ├── Interpreters/           ← Context, executeQuery, DDL interpreter
│   ├── Processors/             ← Pipeline operators (Source/Transform/Sink)
│   ├── Storages/               ← IStorage + MemoryStorage + FileStorage
│   ├── Databases/              ← IDatabase + DatabaseManager
│   ├── Disks/                  ← IDisk + LocalFileDisk + S3Disk
│   ├── IO/                     ← Compression (LZ4, ZSTD), serialization
│   ├── Server/                 ← HTTP server, TCP server, metrics
│   ├── Coordination/           ← Raft consensus (stub)
│   ├── Backups/                ← Backup infrastructure (stub)
│   ├── Loggers/                ← Logging (ConsoleTarget, FileTarget)
│   └── Common/                 ← Settings, exceptions, thread pool, types
├── tests/                      ← Unit tests (Catch2)
├── benchmark/                  ← Benchmarks (Google Benchmark)
├── cmake/                      ← Build helpers
│   ├── dependencies.cmake      ← External deps management
│   ├── compiler.cmake          ← Compiler flags, warnings
│   └── utils.cmake             ← Build utilities
├── configs/                    ← Configuration templates
├── docker/                     ← Docker support
├── public/                     ← Web UI (index.html)
└── docs/                       ← This directory
```

### How to Find Something

| "I want to..." | Look here... |
|------|-----|
| Change how data is stored | `src/Storages/` + `src/Columns/` |
| Add a new SQL keyword | `src/Parsers/parser_query.cpp` (grammar rules) |
| Add a new function | `src/Functions/` — implement `IFunction` + register in `FunctionFactory` |
| Add a new aggregate | `src/AggregateFunctions/` — implement `IAggregateFunction` + register |
| Change query execution | `src/Processors/` + `src/Interpreters/` |
| Modify server endpoints | `src/Server/http_handler.cpp` |
| Add a new data type | `src/DataTypes/` — add concrete type + register in `DataTypeFactory` |
| Change compression | `src/IO/` — codec implementations |
| Fix a parser bug | `src/Parsers/` — start with `parser_query.cpp` |
| Understand data flow | `src/Core/` — `Block`, `Column`, `Field` |

---

## Key Abstractions (The Ones That Matter Most)

### Block — The Data Container

```cpp
// A block is a table: N columns, M rows, each column has the same row count
class Block {
    std::vector<ColumnPtr> columns_;
    std::unordered_map<std::string, size_t> column_index_;
};
```

**Mental model:** A block is a spreadsheet in memory. Every column has the same number of rows. Data flows through the pipeline as a sequence of blocks.

### Column — The Storage Unit

```cpp
class Column {
    virtual size_t size() = 0;
    virtual void insert(Field value) = 0;
    virtual Field get(size_t index) = 0;
};
```

Concrete types: `ColumnVector<T>`, `ColumnString`, `ColumnArray`. Each column stores its data contiguously (hence "columnar").

### Field — The Value Type

A variant-like type holding a single cell value: `Int64`, `Float64`, `String`, `Date`, `bool`, or `null`.

### Context — The Global State

```cpp
class Context {
    std::unordered_map<std::string, IStoragePtr> storages_;
    std::unordered_map<std::string, IDatabasePtr> databases_;
    Settings settings_;
};
```

Every query gets a `Context` that holds references to all databases, storages, and settings.

### Processor — The Pipeline Stage

```cpp
class Processor {
    virtual void start() = 0;
    virtual std::optional<Block> result() = 0;
};
```

Types: `ScanProcessor` (reads storage), `FilterProcessor` (WHERE), `ProjectProcessor` (SELECT columns), `GroupByProcessor` (GROUP BY), `SortProcessor` (ORDER BY), `LimitProcessor` (LIMIT), `InsertProcessor`, etc.

### Factory Pattern — The Extensibility Mechanism

Every extensible component uses a singleton factory:

```cpp
// Adding a new function:
FunctionFactory::instance().register_function("my_func", [](args) {
    return std::make_shared<MyFunction>(args);
});

// Adding a new storage engine:
StorageFactory::instance().register_engine("MyStorage", [](path) {
    return std::make_shared<MyStorage>(path);
});
```

---

## Common Tasks

### 1. Adding a New Scalar Function

1. Create `src/Functions/my_function.h` implementing `IFunction`
2. Implement `execute()` to operate on `Field` values
3. Register in `src/Functions/function_factory.cpp`:
   ```cpp
   FunctionFactory::instance().register_function("my_function", createMyFunction);
   ```
4. Add tests in `tests/test_functions.h`

### 2. Adding a New Data Type

1. Create `src/DataTypes/data_type_mytype.h` inheriting from `IDataType`
2. Implement `create_column()`, `get_name()`, `serialize()`, etc.
3. Register in `src/DataTypes/data_type_factory.cpp`:
   ```cpp
   DataTypeFactory::instance().register_type(std::make_shared<DataTypeMyType>());
   ```
4. Add corresponding column type in `src/Columns/`

### 3. Adding a New Processor

1. Create `src/Processors/my_processor.h` inheriting from `Processor`
2. Implement `start()` and `result()`
3. Register in the processor factory in `src/Interpreters/block_interpreter.cpp`:
   ```cpp
   case PlanNode::Type::MY_TYPE:
       return std::make_shared<MyProcessor>(plan, context);
   ```

### 4. Adding a New Storage Engine

1. Create `src/Storages/my_storage.h` implementing `IStorage`
2. Implement `read()`, `write()`, `columns()`, etc.
3. Register in `src/Storages/storage_factory.cpp`:
   ```cpp
   StorageFactory::instance().register_engine("MyStorage", createMyStorage);
   ```

### 5. Running Tests

```bash
cd build
ctest --output-on-failure          # Run all tests
ctest -R test_functions           # Run only function tests
ctest -R test_parser --verbose    # Run parser tests with verbose output
```

### 6. Running Benchmarks

```bash
cd build
./benchmark/mnemosyne_benchmark  # Run all benchmarks
./benchmark/mnemosyne_benchmark --benchmark_filter=scan  # Run specific benchmark
```

---

## Code Conventions

### Naming

- **Classes:** `PascalCase` — `ColumnVector`, `QueryParser`, `StorageFactory`
- **Methods:** `camelCase` — `getColumnName()`, `executeQuery()`
- **Variables:** `snake_case` — `row_count`, `column_names`
- **Namespaces:** `mnesso::` — e.g., `mnesso::core::Block`
- **Headers:** `snake_case.h` — `column_vector.h`, `query_parser.h`
- **Source files:** `snake_case.cpp` — `column_vector.cpp`, `query_parser.cpp`
- **Constants:** `UPPER_SNAKE_CASE` — `DEFAULT_BLOCK_SIZE`, `MAX_COLUMNS`

### Smart Pointers

- Use `std::shared_ptr` for shared ownership (almost everywhere in this codebase)
- Use `std::unique_ptr` for single ownership (factory creators, local variables)
- Use `std::weak_ptr` when you need to break cycles (storage engines referencing contexts)
- Prefer `std::make_shared` / `std::make_unique`

### Modern C++23 Patterns

- Use `std::expected<T, E>` for fallible operations (returning errors)
- Use `std::span<T>` for non-owning array views (zero-copy)
- Use `std::string_view` for function parameters (avoid string copies)
- Use `std::format` for string formatting (not `sprintf`)
- Use `std::source_location` for debugging/logging

### Error Handling

- Use `common::Exception` with `ErrorCode` enum for errors
- Never throw raw `std::exception` — always use the custom exception type
- Use `std::optional` for "may not have a result" cases
- Use `std::expected` for fallible operations with error details

### Logging

- Use `mnesso::loggers::Logger` for all logging
- Categories: `trace`, `debug`, `info`, `warn`, `error`, `fatal`
- Always include the component name: `Logger::info("Query executed", "QueryExecutor")`

---

## Gotchas (What Will Trip You Up)

### 1. The Analyzer and Planner are Mostly Stubbed

The `Analyzer` validates query structure but **table/column resolution is incomplete**. The `Planner` builds the DAG structure but **join ordering, predicate pushdown, and index selection are stubs**. If you're debugging a query, check whether the issue is in the parser (syntax) or the analyzer/planner (semantics).

### 2. Processor Execution Logic is Stubbed

Processors (`ScanProcessor`, `FilterProcessor`, etc.) are created but their `start()` and `result()` methods are **stub implementations**. The actual query execution pipeline is not fully wired up yet. This is the single biggest gap in the codebase.

### 3. `mnesso::` Not `mnemosyne::`

The namespace is `mnesso::`, not `mnemosyne::`. This is intentional (shorter for code, the project name is Mnemosyne). A common mistake is to use `mnemosyne::` and get compilation errors.

### 4. Shared Pointer Everywhere

The codebase uses `std::shared_ptr` aggressively. If you see memory leaks or reference cycles, check for circular references between contexts and storages. Use `std::weak_ptr` to break cycles.

### 5. Block Column Indexing is Case-Sensitive

Column names in a `Block` are case-sensitive. `SELECT name FROM t` and `SELECT Name FROM t` will fail differently — one resolves the column, the other doesn't.

### 6. CMake FetchContent Dependencies

The project uses CMake's `FetchContent` for `cpp-httplib`, `lz4`, and `zstd`. If you're offline or behind a proxy, these downloads will fail. Run `cmake --configure` once while online, then you can build offline.

### 7. Port 1143 Conflicts

The HTTP server defaults to port 1143. If it's already in use:
- Windows: `netstat -ano | findstr :1143` → `taskkill /PID <pid> /F`
- Linux: `lsof -i :1143` → `kill <pid>`
- Or edit `programs/server/main.cpp` to change the port

### 8. Test File Location

Tests live in `tests/` as individual header files (`test_parser.h`, `test_functions.h`, etc.) that are compiled into a single test binary. Don't create `.cpp` test files — use `.h` files and include them in the test compilation unit.

### 9. C++23 Compiler Requirements

The codebase requires C++23. Older compilers will fail on `std::expected`, `std::format`, and other C++23 features. If you get C++23 errors, check your compiler version first.

### 10. The Web UI is a Static HTML File

`public/index.html` is a dark-themed UI served as a static file by the HTTP server. It's not a full application — many buttons and features are placeholder UI. Don't expect the UI to be fully functional.

---

## Development Workflow

### Recommended Workflow

1. **Start with the docs** — read [ARCHITECTURE.md](ARCHITECTURE.md) for the big picture
2. **Build and run** — get the server working on your machine
3. **Run the tests** — `ctest` to see what's working
4. **Pick a subsystem** — start with one layer (e.g., Functions, or Columns)
5. **Read the code** — follow the factory pattern: find the interface, then find the implementations
6. **Add a test** — write a unit test for your change before implementing it
7. **Run the benchmark** — check that your change doesn't regress performance

### Where to Start Contributing

Good entry points for new contributors:
- **Function implementations** — add new scalar or aggregate functions
- **Data type additions** — add new types (Date, DateTime, UUID, etc.)
- **Parser grammar** — extend the SQL grammar (JOINs, subqueries)
- **Storage engines** — implement new storage backends
- **Tests** — the test suite needs more coverage
- **Documentation** — the docs are a living target

---

## Quick Reference

| Concept | Key Type | Location |
|---------|----------|----------|
| Data container | `core::Block` | `src/Core/block.h` |
| Column storage | `columns::ColumnVector<T>` | `src/Columns/column_vector.h` |
| Single value | `core::Field` | `src/Core/field.h` |
| Type system | `datatypes::IDataType` | `src/DataTypes/i_data_type.h` |
| SQL parser | `parsers::QueryParser` | `src/Parsers/parser_query.h` |
| Query analyzer | `analyzer::Analyzer` | `src/Analyzer/analyzer.h` |
| Query planner | `planner::Planner` | `src/Planner/planner.h` |
| Execution context | `interpreters::Context` | `src/Interpreters/context.h` |
| Pipeline processor | `processors::Processor` | `src/Processors/processor.h` |
| Storage engine | `storages::IStorage` | `src/Storages/i_storage.h` |
| Function registry | `functions::FunctionFactory` | `src/Functions/function_factory.h` |
| Aggregate registry | `aggregate_functions::AggregateFunctionFactory` | `src/AggregateFunctions/aggregate_function_factory.h` |
| Server | `server::Server` | `src/Server/server.h` |
| Logging | `loggers::Logger` | `src/Loggers/logger.h` |
| Settings | `common::Settings` | `src/Common/settings.h` |

---

## Further Reading

- [ARCHITECTURE.md](ARCHITECTURE.md) — full system architecture
- [API.md](API.md) — module API reference
- [GRAMMAR.md](GRAMMAR.md) — supported SQL dialect
- [TECHNICAL_ANALYSIS.md](TECHNICAL_ANALYSIS.md) — deep dive into every subsystem
- [STARTUP.md](../STARTUP.md) — startup and troubleshooting guide
- [README.md](../README.md) — project overview and roadmap

---

*This guide is a living document. If you find something inaccurate or want to add to it, open a PR.*
