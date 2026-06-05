# Repo Inventory Manifest: Mnemosyne

**Scanned:** 2026-06-02
**Source:** Local — `C:\Users\tsuma.thomas\Documents\Mnemosyne`
**Primary language(s):** C++23 (CXX), C
**Runtime target:** Linux/macOS/Windows (native binary, no JS runtime)

---

## 1. What Is This?

Mnemosyne is a **column-oriented analytical database management system** written in C++23, inspired by ClickHouse's architecture. It provides a full query stack: SQL parsing (lexer + recursive-descent parser), semantic analysis, query planning (DAG of plan nodes), pipeline execution (processors), and storage backends (in-memory and file-based). It ships with an HTTP server (port 1143) and a web UI (`public/index.html`) for querying, plus a CLI REPL client. The project targets analytical workloads — OLAP-style scans, aggregations, and filtering — with a focus on columnar data layout for memory efficiency.

---

## 2. Tech Stack

| Layer | Technology |
|-------|-------|
| Language | C++23 (CXX) |
| Build system | CMake 3.28+ |
| Compiler | MSVC 2022 (17.8+), GCC 14+, Clang 18+ |
| Package manager | CMake FetchContent (httplib, lz4, zstd) |
| Test framework | GoogleTest (via CTest) |
| Linter/formatter | clang-tidy + clang-format (LLVM style) |
| Container | Docker (multi-stage build) |
| CI | GitHub Actions (ubuntu-24.04 + macos-14, sanitizers) |
| HTTP server | cpp-httplib v0.18.0 (single-header, fetched at build time) |
| Compression | LZ4 v1.10.0, ZSTD v1.5.6 (fetched at build time) |
| Web UI | Vanilla HTML/CSS/JS (dark theme, JetBrains Mono) |

---

## 3. Directory Map

```
<repo-root>/
├── src/                    # [CORE SOURCE] 14-layer DBMS engine
│   ├── Core/               # Block, Column, Field, Series — data containers
│   ├── Columns/            # ColumnVector, ColumnString, ColumnArray — column impls
│   ├── DataTypes/          # DataType, DataTypeNumber, DataTypeString, factory
│   ├── Parsers/            # Lexer, recursive-descent parser, AST, grammar.y
│   ├── Analyzer/           # Semantic analysis, IQueryTreeNode IR, passes
│   ├── Planner/            # Query optimizer, ExecutionPlan DAG, PlanNode
│   ├── Functions/          # Scalar function registry (arithmetic, comparison)
│   ├── AggregateFunctions/ # Aggregate registry (count, sum, avg, min, max)
│   ├── Interpreters/       # Interpreter, Context, BlockInterpreter, query executor
│   ├── Processors/         # Pipeline processors (Source, Filter, Project, GroupBy, Sort, etc.)
│   ├── Storages/           # IStorage, MemoryStorage, FileStorage, DictionaryStorage, factory
│   ├── Databases/          # IDatabase, Database, DatabaseManager, factory
│   ├── Disks/              # Disk abstraction (LocalFileDisk, S3Disk)
│   ├── IO/                 # Codecs (Native, LZ4, ZSTD, compression)
│   ├── Server/             # HTTPServer, HTTPHandler, TCPServer, HTTP types
│   ├── Coordination/       # Cluster coordination (stub)
│   ├── Backups/            # Backup/restore (stub)
│   ├── Loggers/            # Logger, TargetConsole, TargetFile
│   └── Common/             # Settings, Exceptions, ThreadPool, types, span_compat
├── programs/               # [BINARIES] Server + CLI client
│   ├── server/             # mnemosyne_server (HTTP + TCP, port 1143)
│   └── client/             # MnemosyneClient REPL
├── tests/                  # [TESTS] Unit tests (gtest/ctest), per-subsystem headers
├── benchmark/              # [BENCHMARKS] Performance benchmarks
├── docs/                   # [DOCS] API, ARCHITECTURE, GRAMMAR, skill docs
├── public/                 # [UI] Web UI (index.html, dark theme)
├── docker/                 # [INFRA] Dockerfile + docker-compose.yml
├── configs/                # [CONFIG] mnemosyne.example.yml
├── notebooks/              # [DEV] Jupyter notebooks (dev scratch)
├── outputs/                # [DEV] Query output/error logs
├── cmake/                  # [BUILD] compiler.cmake, dependencies.cmake, utils.cmake
├── .clang-format           # LLVM style, 100-col, 4-space indent
├── .clang-tidy             # llvm-*, modernize-*, bugprone-*, cppcoreguidelines-*
├── .github/workflows/ci.yml # CI: build (ubuntu+macOS, sanitizers) + lint (clang-tidy)
├── CMakeLists.txt          # Top-level: C++23, FetchContent, subdirs, options
├── README.md               # Project overview, architecture, startup guide
├── STARTUP.md              # Build/run instructions (Windows/WSL/MinGW)
├── IMPLEMENTATION_PLAN.md  # Roadmap & TODO list
├── DECISION_SUPPORT_SYSTEM_FUTURE_PLANS.md # Long-term vision doc
├── FIXES.md                # Bug fix log
├── TEST_QUERIES.md         # Test SQL queries
├── TECHNICAL_ANALYSIS.md   # Deep technical analysis (generated)
├── LICENSE                 # MIT License
└── .gitignore
```

---

## 4. Entrypoints

| Entrypoint | File | Purpose |
|------|------|-----|
| **Server binary** | `programs/server/main.cpp` | `mnemosyne_server` — HTTP (1143) + TCP daemon, initializes Context, StorageFactory, DatabaseFactory, Logger, handles SIGINT/SIGTERM |
| **Client binary** | `programs/client/main.cpp` | `mnemosyne_client` — REPL CLI client (stubbed HTTP connect) |
| **Web UI** | `public/index.html` | Dark-themed query editor, table browser, schema viewer, settings panel |
| **npm package** | N/A | Not applicable — native C++ binary, no npm |
| **Shared library** | N/A | Not published as a library; built as executables via CMake |

---

## 5. Architectural Pattern

**Pattern detected:** Single-repo monolith with layered architecture (ClickHouse-inspired).

The codebase is a **single CMake project** with no package manager (no npm/pip/cargo). All dependencies (httplib, lz4, zstd) are fetched at build time via CMake `FetchContent`. The architecture follows a strict pipeline: `HTTP/TCP → Context → Parser → Analyzer → Planner → Interpreter → Processor → Storage`. Each layer is encapsulated in its own `src/<Layer>/` directory with its own `CMakeLists.txt`. There is no monorepo workspace — it's a flat, single-repo monolith with internal module directories.

---

## 6. Key Subsystems

- **Core Data Engine** (`src/Core/` + `src/Columns/` + `src/DataTypes/`) — Block (table of columns), Column (vectorized data), Field (variant type), Series (SIMD-friendly views), DataType (type registry)
- **SQL Parser** (`src/Parsers/`) — Lexer (tokenizer), recursive-descent parser (SELECT, INSERT, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN), precedence-climbing expression evaluator, `grammar.y` (Yacc/Bison spec)
- **Semantic Analyzer** (`src/Analyzer/`) — Validates queries, resolves table/column references, builds `IQueryTreeNode` IR (SelectNode, TableNode, JoinNode, AggregateNode, FilterNode, SortNode, LimitNode)
- **Query Planner** (`src/Planner/`) — Converts IR to `ExecutionPlan` (DAG of `PlanNode`s), estimates cost, stubbed join ordering/predicate pushdown/index selection
- **Execution Engine** (`src/Interpreters/` + `src/Processors/`) — `Interpreter` orchestrates pipeline, `Context` holds global state, `Processor` hierarchy (Scan, Filter, Project, GroupBy, Sort, Limit, Insert, Create, Drop, Show, Describe, Explain)
- **Storage Layer** (`src/Storages/`) — `IStorage` interface, `MemoryStorage` (in-memory), `FileStorage` (columnar `.bin` files), `DictionaryStorage` (stub), `StorageFactory` (registry)
- **Database Catalog** (`src/Databases/`) — `IDatabase` interface, `Database`, `DatabaseManager` (singleton), `DatabaseFactory`
- **Function Registry** (`src/Functions/` + `src/AggregateFunctions/`) — Scalar functions (`+`, `-`, `*`, `/`, `%`, `=`, `!=`, `<`, `>`, `<=`, `>=`), aggregates (`count`, `sum`, `avg`, `min`, `max`), both via factory pattern
- **Server** (`src/Server/`) — HTTP server (cpp-httplib), TCP server, HTTP handler, serves `public/index.html`
- **Infrastructure** (`src/Loggers/`, `src/Common/`, `src/IO/`, `src/Disks/`) — Logging (console/file), settings, exceptions, thread pool, compression (LZ4/ZSTD), disk abstraction (local/S3)
- **Coordination & Backups** (`src/Coordination/`, `src/Backups/`) — Stub implementations for distributed coordination and backup/restore

---

## 7. External Dependencies (notable)

| Package | Purpose |
|---------|-----|
| `cpp-httplib` (v0.18.0) | Single-header HTTP server library (fetched via FetchContent) |
| `lz4` (v1.10.0) | Compression codec (fetched via FetchContent) |
| `zstd` (v1.5.6) | Compression codec (fetched via FetchContent) |
| `libfmt` (via CI) | `std::format` polyfill for GCC (CI only) |
| Catch2 | Test framework (via `tests/catch2/catch_all.hpp`) |
| GoogleTest | Unit test framework (via CTest) |

---

## 8. Deployment & Infrastructure

- **Build**: CMake 3.28+ with C++23. Supports MSVC (Windows), GCC (Linux/WSL), Clang (macOS/WSL).
- **Container**: Docker multi-stage build (`docker/Dockerfile`) + `docker-compose.yml` for local deployment.
- **CI**: GitHub Actions on `push`/`PR` to `main`/`develop`. Builds on `ubuntu-24.04` + `macos-14` with GCC 13. Runs sanitizers (UBSan/ASan) on Ubuntu, release builds on macOS. Lint step runs `clang-tidy-18` on all `src/*.cpp`.
- **Runtime**: Server listens on port **1143** (HTTP) + TCP. Exposes REST endpoints (`/query`, `/databases`, `/tables/{db}`, `/schema/{db}/{table}`, `/settings`, `/metrics`, `/ping`).
- **Storage**: In-memory (`MemoryStorage`) or file-based (`FileStorage` — columns stored as `.bin` files on disk).
- **Configuration**: `configs/mnemosyne.example.yml` (template config file).

---

## 9. Test Infrastructure

- **Framework**: GoogleTest + CTest (enabled via `-DENABLE_TESTS=ON`)
- **Location**: `tests/` — 20+ test headers (`test_all.cpp`, `test_parsers.h`, `test_analyzer.h`, `test_planner.h`, `test_interpreter.h`, `test_processors.h`, `test_storages.h`, `test_databases.h`, `test_functions.h`, `test_aggregate_functions.h`, `test_data_types.h`, `test_columns.h`, `test_common.h`, `test_io.h`, `test_loggers.h`, `test_coordination.h`, `test_backups.h`, `test_disks.h`, `test_server.h`)
- **Coverage**: Per-subsystem test headers; `tests_list.txt` tracks test modules
- **Benchmark**: `benchmark/` directory with `benchmark_main.cpp` (enabled via `-DENABLE_BENCHMARK=ON`)
- **CI**: Tests run automatically on every PR/merge; results uploaded as artifacts

---

## 10. Open Questions for Deeper Analysis

1. **Processor execution logic**: The `Processor` classes are created but their `start()` and `result()` methods appear to be stubs — what is the actual execution pipeline doing today?
2. **JOIN & subquery support**: The parser grammar and planner have stubbed join handling — is JOIN syntax even parsed, or is it a future TODO?
3. **Transaction & concurrency model**: The codebase mentions `ThreadPool` and mutex protection in storage, but is there any transaction isolation or ACID guarantee?
4. **Web UI completeness**: `public/index.html` is a dark-themed UI with query editor and table browser — how much of it is functional vs. static HTML?
5. **Serialization format**: Column data is stored as `.bin` files — what is the wire format? Is it compatible with ClickHouse's Native protocol?
6. **S3 storage**: `disk_s3.cpp` is listed but appears stubbed — what S3 client/library is intended for production use?
7. **Configuration system**: `mnemosyne.example.yml` exists but is it actually read by the server at startup?

---

*Manifest generated by `/repo-cartographer` — 2026-06-02*