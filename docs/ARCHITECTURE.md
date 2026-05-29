# Mnemosyne Architecture

Mnemosyne is a column-oriented analytical database management system written in C++23.

## Data Flow

```
SQL String
    │
    ▼
┌──────────┐
│  Lexer    │  Tokenize SQL into tokens
└─────┬─────┘
      │
      ▼
┌──────────┐
│  Parser   │  Recursive descent parser → AST
└─────┬─────┘
      │
      ▼
┌──────────┐
│ Analyzer  │  Semantic analysis → Query Tree
└─────┬─────┘
      │
      ▼
┌──────────┐
│ Planner   │  Query Tree → Execution DAG
└─────┬─────┘
      │
      ▼
┌──────────┐
│Interpreter│  DAG → Processor Pipeline
└─────┬─────┘
      │
      ▼
┌──────────┐
│Processors │  Execute pipeline → Block results
└─────┬─────┘
      │
      ▼
   Results
```

## Layer Details

### Layer 1: Core (`Core/`, `DataTypes/`)
- **Block** — rectangular block of columnar data
- **Series** — contiguous storage with arithmetic operations
- **Field** — single cell value (variant type)
- **DataType** — type system with registry
- **Column** — column types (ColumnVector, ColumnString, ColumnArray)

### Layer 2: Parsers (`Parsers/`)
- **Lexer** — tokenizer for SQL keywords, literals, operators
- **Parser** — recursive descent parser implementing full SQL grammar
- **AST** — abstract syntax tree with expression types
- **Grammar** — documented grammar reference

### Layer 3: Analysis & Planning (`Analyzer/`, `Planner/`)
- **Analyzer** — semantic analysis, column resolution, function validation
- **Passes** — type inference, constant folding, column pruning
- **Query Tree** — intermediate representation (IR) of the query
- **Planner** — converts query tree to execution DAG
- **Cost Model** — cost-based optimization
- **Execution Plan** — physical plan with operators

### Layer 4: Execution (`Interpreters/`, `Processors/`)
- **Context** — global query context (settings, databases, storages)
- **Interpreter** — base class for interpreters (SELECT, INSERT, etc.)
- **Processor** — pipeline operator (source, transform, sink)
- **Pipeline** — ordered sequence of processors
- **Query Executor** — manages execution of processors

### Layer 5: Storage (`Storages/`, `Databases/`, `Disks/`, `IO/`)
- **IStorage** — abstract storage interface
- **StorageEngine** — implementations (File, Memory, Dictionary, S3)
- **DatabaseEngine** — database management (Memory, MySQL, File, Namespace)
- **IDisk** — disk back-end (LocalFile, S3, LocalDisk)
- **CompressionCodec** — compression (LZ4, ZSTD, Native)

### Layer 6: Infrastructure (`Loggers/`, `Server/`, `Coordination/`, `Backups/`, `Common/`)
- **Logger** — logging infrastructure (ConsoleTarget, FileTarget)
- **HTTPServer** — HTTP server with request handler
- **TCPServer** — TCP server for MySQL-compatible protocol
- **Server** — high-level server manager
- **Coordination** — distributed consensus (Raft-based)
- **Backup** — backup manager
- **ThreadPool** — work-stealing thread pool
- **Settings** — global configuration

## Module Dependencies

```
Core → DataTypes → Columns
Parsers → AST → Core
Analyzer → Parsers + Core
Planner → Analyzer + Core
Interpreters → Planner + Core + Processors
Processors → Core + Columns
Storages → Core + Data Types + Disks
Databases → Storages + Core
Disks → Core + IO
IO → Core
Loggers → Core
Server → Interpreters + Loggers
Coordination → Core
Backups → Storages + Disks
Common → [none]
```

## Design Patterns

- **Plugin Architecture** — storage engines, database engines, and codec factory as singleton registries
- **Visitor Pattern** — AST node traversal and analysis
- **Command Pattern** — query execution as command objects
- **Pipeline Pattern** — processor pipeline for query execution
- **Observer Pattern** — event handling in coordination
- **Strategy Pattern** — different compression codecs
- **Singleton Pattern** — registry factories (DataType, Storage, Codec)
- **Factory Pattern** — interpreter creation, storage creation

## Performance Considerations

1. **Columnar Storage** — data layout optimized for analytical queries
2. **SIMD Acceleration** — SIMD instructions for vector operations
3. **Work-Stealing** — efficient thread pool for parallel execution
4. **Zero-Copy** — span/string_view for non-owning references
5. **Memory Pool** — object pooling for allocation efficiency
6. **Compression** — LZ4 for fast compression, ZSTD for high ratio
7. **Caching** — cache for frequently accessed data

## Testing Strategy

- **Unit Tests** — Catch2 tests for individual components
- **Integration Tests** — full query execution tests
- **Benchmark** — Google Benchmark for performance measurement
- **Coverage** — test coverage tracking

## Documentation

- **Doxygen** — API documentation
- **Architecture** — this document
- **Grammar** — SQL grammar reference
- **Examples** — usage examples
