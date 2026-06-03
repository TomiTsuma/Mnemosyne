# ClickHouse Architecture Analysis

Deep-dive guide for analyzing how ClickHouse's code works — tracing data flow, control flow, and architectural patterns.

## Entry Points

### Server Startup Flow

```
programs/server/server.cpp (main)
  └── Application initialization (Core/Settings)
      ├── registerFunctions()        — register 500+ scalar functions
      ├── registerAggregateFunctions() — register 200+ aggregate functions
      ├── registerBuiltinFunctions()  — register built-in function set
      ├── StorageFactory::register()  — register storage engines
      ├── TableFunctionFactory::register() — register table functions
      ├── HTTPServer::start()        — HTTP interface
      ├── TCPServer::start()         — TCP interface
      ├── Keeper::start()            — ZooKeeper coordination
      └── AsynchronousMetrics::start() — background metrics collection
```

### Client Query Flow

```
Client connection (TCP/HTTP)
  └── Client::connect()
      └── receivePacket()
          └── Server::receiveQuery()
              └── InterpretCreateQuery / InterpretAlterQuery
                  └── executeQuery()  ← main entry point
                      ├── Parser → AST (Parsers/)
                      ├── Analyzer (Analyzer/)
                      ├── Planner (Planner/)
                      ├── Interpreter (Interpreters/)
                      └── Processor pipeline execution (Processors/)
```

### Query Execution Stack

1. **Parsing** (`src/Parsers/`)
   - `ParserQuery.cpp` → `ASTPtr` (Abstract Syntax Tree)
   - ~100+ AST node types (ASTSelectQuery, ASTCreateQuery, etc.)
   - ANTLR-generated grammar for SQL dialect

2. **Analysis** (`src/Analyzer/`)
   - `AnalyzerPasses.cpp` — validation and transformation passes
   - Translates AST to internal `IQueryTreeNode` representation
   - Resolves table/column names, computes types

3. **Planning** (`src/Planner/`)
   - `Planner.cpp` — builds execution plan
   - Creates `PlannerNode` DAG
   - Determines parallelism, sharding, replication strategy

4. **Interpretation** (`src/Interpreters/`)
   - `Aggregator.cpp` — distributed aggregation
   - `DatabaseOrdinarityInterceptor.cpp` — ORDINARY database handling
   - `Context.cpp` — query context management
   - `DDLWorker.cpp` — DDL execution on cluster
   - `Loggers.cpp` — query logging

5. **Execution** (`src/Processors/`, `src/QueryPipeline/`)
   - `QueryPipeline::execute()` — runs the pipeline
   - `SourceWithKeyValueOutput` — key-value output
   - `IProcessor` — base processor interface
   - `TransformProcessor` — data transformation
   - `ReadFromMergeTree` — storage read processor

## Key Data Structures

### Block (Core/Block.h)

The fundamental unit of data processing — a table-like structure of columns.

```cpp
struct Block {
    ColumnsWithTypeAndName columns;  // named columns
    NamesAndTypesPairs columnTypes;  // column type metadata
    // Operations: getRowBytes, getBytes, slice, etc.
};
```

### Column (src/Columns/)

Abstract column interface with concrete implementations:

- `IColumn` — base interface
- `ColumnVector<T>` — fixed-size typed array
- `ColumnString` — variable-length strings
- `ColumnArray` — nested arrays
- `ColumnMap` — key-value maps
- `ColumnTuple` — composite tuples
- `ColumnFixedString` — fixed-length strings
- `ColumnDecimal` — decimal types
- `ColumnLowCardinality` — low-cardinality encoding

### Field (Core/Field.h)

Variant type for single values — can hold any ClickHouse type.

### DataType (src/DataTypes/)

Type hierarchy:
```
IDataType
  ├── DataTypeNumber (UInt8/16/32/64, Int8/16/32/64, Int128, UInt128, Int256, UInt256)
  ├── DataTypeFloat (Float32/64)
  ├── DataTypeString (String, FixedString)
  ├── DataTypeDate
  ├── DataTypeDateTime
  ├── DataTypeDateTime64
  ├── DataTypeUUID
  ├── DataTypeArray
  ├── DataTypeMap
  ├── DataTypeTuple
  ├── DataTypeNullable
  ├── DataTypeLowCardinality
  ├── DataTypeNested
  ├── DataTypeEnum
  ├── DataTypeInterval
  ├── DataTypeIP
  ├── DataTypePoint
  ├── DataTypeRing
  └── ...
```

## Storage Engine Architecture

### MergeTree Family (Storages/MergeTree/)

Core storage engine — the backbone of ClickHouse's analytical capabilities.

```
IMergeTreeDataPart
  ├── MergeTreeData
  │   ├── MergeTreeWriter  — batch writing
  │   ├── MergeTreeReader   — block-level reading
  │   ├── MergeTreeDataSelectProcessor  — reading with filtering
  │   └── MergeTreeDataMerger  — background merging
  │
  ├── ReplicatedMergeTree  — distributed replication via NuRaft
  ├── AggregatingMergeTree — materialized views support
  ├── SummingMergeTree     — auto-summing on merge
  ├── CollapsingMergeTree  — event tracking
  ├── VersionedCollapsingMergeTree
  ├── GraphiteMergeTree    — time-series aggregation
  └── ... (20+ variants)
```

### Storage Part Layout

```
part_name/
  ├── checksums.txt     — file sizes and CRC32
  ├── columns.txt       — column list and versions
  ├── count.txt         — row count
  ├── primary.idx       — primary key index (granules)
  ├── minmax_*.idx      — min/max for each column
  ├── data.bin/.gz      — compressed column data
  ├── default_compression_codec.codec  — codec metadata
  └── ...
```

### Granule System

- Data is divided into **granules** (typically 8192 rows)
- Primary key index maps granule ranges to values
- Query filters skip non-matching granules
- Index types: `index_granularity`, `index_granularity_bytes`

## Distributed Query Processing

### Cluster Architecture

```
Client
  └── Distributed table engine
      ├── Shard 1 (ReplicatedMergeTree)
      ├── Shard 2 (ReplicatedMergeTree)
      └── Shard N (ReplicatedMergeTree)
```

### Distributed Execution

1. `StorageDistributed` — routes queries to shards
2. `RemoteQueryExecutor` — sends partial queries to remote servers
3. `DistributedAsyncInsert` — async batch insertion
4. `ClusterProxy` — cluster communication layer

### Consensus (Coordination/)

- `KeeperConnection` — ZooKeeper protocol over TCP
- `InMemoryLogStore` — in-memory log store
- `NuRaft` — Raft consensus implementation
- `KeeperServer` — coordination server
- `FourLetterCommand` — ZooKeeper four-letter commands (srvr, ruok, conf, etc.)

## Function System

### Scalar Function Registration

```cpp
// In src/Functions/FunctionX.cpp
FunctionRegistration<FunctionX>::registerFunction();

// Usage: SELECT myFunc(col1, col2) FROM table
```

### Function Categories

| Category | Files | Count |
|----------|-------|-------|
| Math | `Function*Arithmetic.h` | ~50 |
| String | `Function*String.h` | ~80 |
| Date/Time | `Function*DateTime*.h` | ~60 |
| Comparison | `Function*Comparison.h` | ~30 |
| Logical | `Function*Logical.h` | ~20 |
| Conditional | `Function*Conditional.h` | ~25 |
| Array | `Function*Array.h` | ~40 |
| Map | `Function*Map.h` | ~30 |
| Hash | `Function*Hash.h` | ~15 |
| Encoding | `Function*Base64*.h` | ~10 |
| AI | `FunctionBaseAI.cpp` | ~5 |
| IP | `Function*IP*.h` | ~15 |
| Geo | `Function*Geo*.h` | ~20 |

### Aggregate Function System

```cpp
// Base class for aggregate functions
class IAggregateFunctionHelper : public IAggregateFunction
{
    // State allocation, destruction
    // add() — accumulate a row
    // merge() — merge two states
    // serialize() / deserialize()
    // getResult() — final result
};
```

## I/O Architecture (src/IO/)

### Network

- `ReadBuffer` / `WriteBuffer` — buffered I/O base classes
- `ReadBufferFromHTTP` — HTTP downloads
- `WriteBufferFromHTTP` — HTTP uploads
- `ReadBufferFromAyncIOPool` — async I/O
- `Socket` — TCP socket abstraction
- `InetAddress` — network address handling

### Compression

- `CompressedReadBuffer` / `CompressedWriteBuffer` — on-the-fly compression
- `Codec` interface: `createDecoder()` / `createEncoder()`
- Codecs: LZ4, ZSTD, Deflate, Delta, DoubleDelta, Gorilla, T64, Gzip, etc.

### Object Storage

- `ReadBufferFromS3` / `WriteBufferFromS3` — AWS S3
- `ReadBufferFromAzureBlobStorage` — Azure Blob
- `ReadBufferFromHDFS` — HDFS
- `ObjectStorage` — unified object storage interface
- `Disk` — unified disk interface

## Disk Abstraction (src/Disks/)

```
IDisk (base interface)
  ├── DiskLocal       — local filesystem
  ├── DiskS3          — AWS S3
  ├── DiskAzureBlob   — Azure Blob
  ├── DiskHDFS        — HDFS
  ├── DiskNetwork     — network disk
  ├── DiskEncrypted   — transparent encryption
  ├── DiskCache       — caching layer
  ├── DiskRemote      — remote disk
  └── DiskObjectStorage — object storage abstraction
```

## Settings System (src/Common/BaseSettings/)

All configuration is managed through `BaseSettings`:

```cpp
// Definition
struct Settings
{
    SettingUInt64 max_threads;
    SettingString format_schema_path;
    // ... ~600+ settings
};

// Usage
context->getSettings()[SettingMaxThreads] = 32;
```

### Settings Categories

| Category | Examples |
|----------|----------|
| Query | `max_threads`, `max_memory_usage`, `max_execution_time` |
| Performance | `optimize_move_to_prewhere`, `max_block_size` |
| Format | `output_format_json_quote_denormals` |
| Storage | `storage_policy`, `move_to_prewhere` |
| Network | `http_max_uri_size`, `max_connections` |
| Security | `allow_ddl`, `allow_introspection_functions` |
| Distributed | `distributed_product_mode`, `max_shard_rows` |

## Threading Model

### Background Work

- `BackgroundSchedulePool` — scheduled background tasks
- `BackgroundJobsAssignee` — manages background merges, mutations, moves
- `BackgroundProcessingPool` — pool for background tasks
- `ThreadPool` — thread pool abstraction

### Parallelism

- `ParallelReplicas` — parallel scan across replicas
- `DistributedParallel` — parallel distributed execution
- `AggregateFunction` — parallel aggregation with intermediate state
- `HashJoin` — parallel hash join

## Memory Management

### Allocator (src/Common/Allocator.cpp)

- Custom `MemoryAllocator` with memory tracking
- `Arena` — bump allocator for temporary data
- `AllocationInterceptors` — memory usage monitoring
- jemalloc integration (default on Linux)

### Memory Tracking

- `MemoryTracker` — per-query memory accounting
- `ProcessorsMemoryTracker` — per-processor tracking
- `AsynchronousMetrics` — system-wide metrics
- `SystemLog` — memory usage logging

## Plugin System

### Factory Registration

```cpp
// Function registration
FunctionFactory::instance().register<FunctionX>("func_name");

// Storage registration
StorageFactory::instance().register("MergeTree", createMergeTree);

// Table function registration
TableFunctionFactory::instance().register("table_function", create);

// Codec registration
CompressionCodecFactory::instance().register<LZ4>();
```

### Extension Points

1. **Functions** — add new scalar functions
2. **Aggregate Functions** — add new aggregation
3. **Storage Engines** — add new table types
4. **Data Types** — add new column types
5. **Codecs** — add new compression codecs
6. **Formats** — add new data formats
7. **Disks** — add new storage backends
8. **Databases** — add new database engines
9. **Table Functions** — add new table sources
10. **Functions** — add new SQL functions

## Architecture Patterns

### 1. Pipeline-Based Execution

```cpp
// Each processor has input and output ports
auto processor = std::make_shared<ReadFromMergeTree>(storage, query_info);
auto transform = std::make_shared<AggregatingTransform>(header);
auto sink = std::make_shared<AggregatedDataVariants>(header);

// Chain processors
processor->connect(transform);
transform->connect(sink);

// Execute
pipeline.execute();
```

### 2. AST-Based SQL Processing

```
SQL String → Parser → AST → Analyzer → IQueryTreeNode → Planner → Processor Pipeline
```

### 3. Columnar Data Processing

```
Block (table of columns)
  ├── ColumnVector<UInt64>  (column A)
  ├── ColumnString           (column B)
  └── ColumnArray<UInt64>    (column C)
```

### 4. Multi-Protocol Server

```
Server (programs/server/)
  ├── HTTPHandler      — HTTP protocol
  ├── TCPHandler       — TCP protocol
  ├── MySQLHandler     — MySQL-compatible
  ├── PostgresHandler  — PostgreSQL-compatible
  ├── GRPCServer       — gRPC protocol
  └── WebSocketHandler — WebSocket protocol
```

### 5. Distributed Consensus

```
Keeper (programs/keeper/)
  ├── NuRaft consensus
  ├── InMemoryLogStore
  ├── SnapshotManager
  ├── KeeperConnection (client)
  └── FourLetterCommand (diagnostics)
```

## Key Files for Deep Dive

### Core Processing

| File | Purpose |
|------|---------|
| `src/Interpreters/executeQuery.cpp` | Main query execution entry |
| `src/Interpreters/Context.cpp` | Query context and settings |
| `src/Processors/QueryPipeline.cpp` | Pipeline execution |
| `src/Storages/MergeTree/MergeTreeData.cpp` | Core storage engine |
| `src/Parsers/ParserQuery.cpp` | SQL parser entry |
| `src/Analyzer/AnalyzerPasses.cpp` | Query analysis |

### Storage

| File | Purpose |
|------|---------|
| `src/Storages/MergeTree/MergeTreeWriter.cpp` | Block writing |
| `src/Storages/MergeTree/MergeTreeReader.cpp` | Block reading |
| `src/Storages/MergeTree/MergeTreeDataSelectProcessor.cpp` | Select execution |
| `src/Storages/MergeTree/MergeTreeDataMerger.cpp` | Background merging |
| `src/Storages/MergeTree/MergeTreePartsMigrator.cpp` | Part migration |

### Distributed

| File | Purpose |
|------|---------|
| `src/Storages/StorageDistributed.cpp` | Distributed table engine |
| `src/Interpreters/RemoteQueryExecutor.cpp` | Remote query execution |
| `src/Coordination/KeeperServer.cpp` | Coordination server |
| `src/Coordination/KeeperConnection.cpp` | ZooKeeper client |

### Functions

| File | Purpose |
|------|---------|
| `src/Functions/FunctionFactory.cpp` | Function registration |
| `src/Functions/FunctionHelpers.cpp` | Function utilities |
| `src/Functions/IFunction.h` | Function interface |
| `src/Functions/aggregateFunctionFactory.cpp` | Aggregate function registration |

## Common Patterns to Recognize

1. **`IQueryTreeNode`** — intermediate query representation
2. **`Block`** — data unit flowing through processors
3. **`Column`** — individual column data structure
4. **`Field`** — single value variant type
5. **`Context`** — query execution context
6. **`Settings`** — configuration parameters
7. **`Processor`** — execution stage
8. **`Source`** — data source processor
9. **`Sink`** — data destination processor
10. **`Transform`** — data transformation processor
11. **`Storage`** — table storage engine
12. **`DataPart`** — physical data storage unit
13. **`Granule`** — data partition within a part
14. **`Codec`** — compression codec
15. **`Disk`** — storage backend abstraction
16. **`ObjectStorage`** — cloud storage abstraction
17. **`Keeper`** — coordination service
18. **`NuRaft`** — Raft consensus implementation
19. **`BackgroundJobs`** — background task management
20. **`MemoryTracker`** — memory accounting

## Debugging Tips

### Adding Traces

```cpp
// In any source file
LOG_TRACE(log, "Debug message: {}", value);

// Available log levels:
LOG_TRACE   // Trace (most detailed)
LOG_DEBUG   // Debug
LOG_INFO    // Info
LOG_WARNING // Warning
LOG_ERROR   // Error
LOG_FATAL   // Fatal (crashes)
```

### Profiling

```bash
# CPU profiling
clickhouse profile --query="SELECT ..."

# Memory profiling
clickhouse profile --memory --query="SELECT ..."

# Query profiler
SET query_profiler_real_time_period_ns = 1000000;
```

### Query Plan

```sql
EXPLAIN PIPELINE SELECT ...;
EXPLAIN ACTIONS SELECT ...;
EXPLAIN INDEXES SELECT ...;
```

### Settings for Debugging

```sql
SET max_memory_usage = 10000000000;
SET log_queries = 1;
SET log_queries_cut_to_length = 0;
SET max_ast_elements = 0;
SET max_expanded_ast_elements = 0;
```
