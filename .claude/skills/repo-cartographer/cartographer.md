# ClickHouse Codebase Map

A comprehensive map of the ClickHouse open-source column-oriented DBMS codebase.

## Quick Stats

- **Language**: C++23 (~4,238 .cpp files, ~3,594 .h files in src/)
- **Build System**: CMake 3.25+ with Ninja
- **CI**: GitHub Actions + custom "praktika" framework
- **License**: Apache 2.0

## Top-Level Directory Structure

```
ClickHouse/
├── base/              # Low-level utilities, primitives, and musl compat
├── benchmark/         # Single benchmark runner and config
├── ci/                # CI orchestration (praktika framework, workflows, infra)
├── cmake/             # CMake toolchains, flags, sanitizers, platform configs
├── contrib/           # Third-party dependencies (git submodules)
├── docker/            # Docker images and compose files
├── docs/              # Developer-facing documentation source
├── packages/          # RPM/DEB packaging scripts
├── programs/          # Standalone executables (~22 binaries)
├── rust/              # Rust crates (minimal, for specific low-level tasks)
├── src/               # Core ClickHouse source code (~25 modules)
└── tests/             # Integration tests, performance benchmarks, fuzzing
```

## Core Source (`src/`) — Module Map

ClickHouse is organized into ~25 top-level modules in `src/`, each corresponding to a subsystem.

### Query Processing Pipeline

| Module | Purpose |
|--------|---------|
| `Parsers/` | SQL parser (ANTLR-generated AST nodes, ~100+ query types) |
| `Analyzer/` | Query analyzer — translates AST to internal plan representation |
| `Planner/` | Query planner — builds execution plans from analyzed queries |
| `Interpreters/` | Query interpreters — executes AST nodes, manages context (~100+ files) |
| `Processors/` | Processor interface — pipeline-based execution engine |
| `QueryPipeline/` | Query pipeline — parallel execution, channels, adapters |

### Storage Engine

| Module | Purpose |
|--------|---------|
| `Storages/` | Table engine implementations (MergeTree, Log, Memory, etc.) |
| `Disks/` | Storage backend abstraction (local disk, S3, HDFS, proxy) |
| `Databases/` | Database engine abstraction (Atomic, Lazy, MySQL, etc.) |

### Data Model

| Module | Purpose |
|--------|---------|
| `Core/` | Core types — Block, Column, Field, Series, DataTypes |
| `DataTypes/` | Type definitions and registration |
| `Columns/` | Column implementations (vector, fixed, map, array, etc.) |
| `AggregateFunctions/` | 200+ aggregate function implementations |
| `Functions/` | 500+ scalar function implementations |

### I/O and Serialization

| Module | Purpose |
|--------|---------|
| `IO/` | I/O primitives — readers, writers, compressors, network |
| `Formats/` | Data format readers/writers (JSON, CSV, Parquet, Arrow, etc.) |
| `Compression/` | Compression codecs (LZ4, ZSTD, Deflate, etc.) |

### Server and Networking

| Module | Purpose |
|--------|---------|
| `Server/` | HTTP/TCPServer, HTTP/TCP handlers, metrics, diagnostics |
| `Access/` | User authentication, authorization, ACL, credentials |

### Distributed Coordination

| Module | Purpose |
|--------|---------|
| `Coordination/` | ZooKeeper-compatible protocol implementation (keeper) |

### Infrastructure

| Module | Purpose |
|--------|---------|
| `Common/` | Utilities — settings, allocators, threading, logging, macros |
| `Backups/` | Online backup and restore infrastructure |
| `Loggers/` | Logging infrastructure |
| `Examples/` | Example code for developers |

## Programs (`programs/`) — Executable Map

| Program | Description |
|---------|-------------|
| `clickhouse` | Main binary (server + client + keeper via symlink/redirect) |
| `client` | CLI client for connecting to ClickHouse server |
| `server` | ClickHouse server daemon |
| `keeper` | ZooKeeper-compatible coordination service |
| `keeper-bench` | Keeper performance benchmark |
| `keeper-client` | Keeper CLI |
| `keeper-converter` | Converts old keeper data format |
| `keeper-data-dumper` | Dumps keeper data for analysis |
| `keeper-utils` | Keeper utilities |
| `format` | Formats ClickHouse data files |
| `compressor` | Compresses/decompresses data |
| `checksum-for-compressed-block` | Computes block checksums |
| `extract-from-config` | Extracts config values |
| `config_tools` | Configuration management utilities |
| `obfuscator` | Data obfuscation utility |
| `static-files-disk-uploader` | Uploads static files to disk |
| `su` | ClickHouse privilege escalation |
| `zookeeper-dump-tree` | Dumps ZooKeeper tree |
| `zookeeper-remove-by-list` | Removes ZooKeeper paths |
| `git-import` | Git import utility |
| `bash-completion` | Shell completion scripts |

## Build System (`cmake/`)

Key CMake files:

| File | Purpose |
|------|---------|
| `arch.cmake` | Architecture detection (x86_64, aarch64, etc.) |
| `tools.cmake` | Compiler/toolchain detection |
| `sanitize.cmake` | Sanitizer configurations (ASan, UBSan, etc.) |
| `warnings.cmake` | Compiler warning flags |
| `add_warning.cmake` | Warning addition helpers |
| `ccache.cmake` | Compiler cache support |
| `target.cmake` | Target configuration helpers |
| `version.cmake` | Version generation |
| `git.cmake` | Git info integration |
| `linux/default_libs.cmake` | Linux-specific library links |
| `darwin/default_libs.cmake` | macOS-specific library links |
| `freebsd/default_libs.cmake` | FreeBSD-specific library links |
| `toolchain/` | Cross-compilation toolchains (ARM, s390x, RISC-V, etc.) |

## Dependencies (`contrib/`)

Major dependency groups:

| Category | Key Dependencies |
|----------|-----------------|
| **Networking** | aws-sdk, azure-sdk, curl, grpc |
| **Serialization** | arrow, avro, capnproto, protobuf |
| **Compression** | brotli, bzip2, lz4, zstd, snappy |
| **Crypto** | openssl, crypto-crypto |
| **Data Structures** | abseil-cpp, boost, roaring (croaring) |
| **Coordination** | NuRaft |
| **Parsing** | antlr4 |
| **Math** | gammatime, libdivide, consistent-hashing |
| **Other** | pcre2, re2, simdjson, simd, StringZilla, SimSIMD |

## Testing Infrastructure (`tests/`)

| Directory | Purpose |
|-----------|---------|
| `integration/` | pytest-based integration tests (~1000+ tests) |
| `performance/` | Performance regression tests |
| `perf_drafts/` | Experimental performance tests |
| `fuzz/` | Fuzzing configurations |
| `ci/` | Test infrastructure utilities |
| `clickhouse-test` | Python test runner script |
| `config/` | Test configuration files |
| `queries/` | SQL test queries (correctness tests) |
| `benchmarks/` | Benchmark test suite |
| `lexer/` | Lexer unit tests |

## CI System (`ci/`)

| Component | Purpose |
|-----------|---------|
| `praktika/` | Custom CI framework (Python) |
| `workflows/` | Workflow definitions (23+ workflows) |
| `jobs/` | Individual CI job definitions |
| `docker/` | CI Docker images |
| `settings/` | Workflow configuration |
| `infra/` | Infrastructure management |
| `defs/` | Shared definitions |

Key workflows: `pull_request.yml`, `master.yml`, `nightly_*.yml`, `release_branches.yml`, `optimize_clickhouse.yml`

## GitHub CI (`ci/workflows/` / `.github/workflows/`)

| Workflow | Purpose |
|----------|---------|
| `pull_request.yml` | PR build and test pipeline |
| `master.yml` | Main branch nightly build |
| `nightly_coverage.yml` | Code coverage collection |
| `nightly_fuzzers.yml` | Fuzzing tests |
| `nightly_jepsen.yml` | Jepsen consistency tests |
| `nightly_keeper.yml` | Keeper stress tests |
| `nightly_statistics.yml` | Performance statistics |
| `release_branches.yml` | Release branch builds |
| `custom_build_praktika.yml` | Custom CI builds |
| `vectorsearchstress.yml` | Vector search stress tests |

## Packaging (`packages/`)

- **RPM**: `clickhouse-server.yaml`, `clickhouse-keeper.yaml`, `clickhouse-rpm.repo`
- **DEB**: `clickhouse-server.yaml`, `clickhouse-keeper.yaml`
- **Init**: `clickhouse-server.init`, `clickhouse-server.service`
- **Postinstall**: `clickhouse-server.postinstall`, `clickhouse-keeper.postinstall`

## Docker (`docker/`)

- `compose/` — Docker Compose configurations for clusters
- Dockerfiles for official images
- Test environment configurations

## Key Architectural Patterns

1. **Block-oriented processing**: Data flows through the pipeline as `Block` objects (columns of rows)
2. **Pipeline processors**: Each SQL operation is a `Processor` that reads from input, writes to output
3. **AST-based SQL**: SQL is parsed into a rich AST (`ASTCreateQuery`, `ASTSelectQuery`, etc.)
4. **MergeTree family**: Core storage engine with variants (ReplicatedMergeTree, AggregatingMergeTree, etc.)
5. **ZooKeeper coordination**: Distributed features rely on ZooKeeper (now NuRaft-based keeper)
6. **Column-oriented**: Data is processed in column chunks for analytical workloads
7. **Plugin-style functions**: Functions and aggregate functions are registered dynamically
8. **Multi-protocol server**: HTTP, TCP, MySQL-compatible, gRPC, and gRPC-HTTP interfaces
