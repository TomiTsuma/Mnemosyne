# ClickHouse Table Operations Documentation

## 1. Architecture Overview

```
SQL String
    │
    ▼
┌────────────┐
│   Lexer    │  Tokenizes SQL into tokens
└─────┬──────┘
      │
      ▼
┌────────────┐
│   Parser   │  Recursive‑descent parser → AST
└─────┬──────┘
      │
      ▼
┌────────────┐
│ Interpreter│  Executes AST nodes (CREATE, INSERT, …)
└─────┬──────┘
      │
      ▼
┌────────────┐
│ Processors│  Pipeline that touches storages
└────────────┘
```

*Mnemosyne* follows a similar three‑layer flow (Lexer → Parser → Interpreter) but lacks the full DDL‑guard and storage‑factory layers present in ClickHouse. The diagram above mirrors the style used in `docs/ARCHITECTURE.md`.

---

## 2. CREATE TABLE Implementation

### AST Layer
- **Class:** `ASTCreateQuery`
- **Key members:**
  - `ASTStorage * storage` – describes the storage engine.
  - `ASTColumns * columns` – list of column definitions.
  - `bool if_not_exists` – flag for `IF NOT EXISTS`.
  - `String database` / `String table` – target identifiers.

### Parser Layer
- **Class:** `ParserCreateQuery`
- **Parsing steps:**
  1. Consume the `CREATE` keyword and optional `TABLE`.
  2. Parse optional `IF NOT EXISTS`.
  3. Parse the qualified table name.
  4. Parse column list inside `(` `)` → `ASTColumns`.
  5. Parse optional `ENGINE = <engine>` → `ASTStorage`.
  6. Build an `ASTCreateQuery` node and attach children.

### Interpreter Layer
- **Class:** `InterpreterCreateQuery`
- **Main entry:** `execute()` → `doCreateTable()`.
- **Key methods:**
  - `createTable(Context & ctx, const ASTCreateQuery & query)` – validates and creates storage.
  - `doCreateTable(Context & ctx, const StoragePtr & storage, const ASTCreateQuery & query)` – registers the table in the database.
  - `getTablePropertiesAndNormalizeCreateQuery()` – resolves default format, column types, and storage settings.
- **Storage Engine Integration:** The engine name from `ASTStorage` is looked up via the **StorageFactory** (`StorageFactory::instance().get(storage_name)`) which returns a factory function constructing the concrete `Storage` implementation.

---

## 3. INSERT Implementation

### AST Layer
- **Class:** `ASTInsertQuery`
- **Members:**
  - `String database`, `String table` – target.
  - `ASTExpressionList * columns` – optional column list.
  - `ASTSelectWithUnionQuery * select` – for `INSERT … SELECT`.
  - `bool has_values` – true when a `VALUES (…)` clause is present.

### Parser Layer
- **Class:** `ParserInsertQuery`
- **Logic:**
  1. Parse `INSERT INTO <table>`.
  2. Optional column list.
  3. Distinguish between `VALUES`, `FORMAT`, or a sub‑`SELECT` query.
  4. Populate `ASTInsertQuery` accordingly.

### Interpreter Layer
- **Class:** `InterpreterInsertQuery`
- **Pipeline construction:**
  - `buildInsertPipeline()` – creates a pipeline that reads the input data, optionally applies a `Format` parser, and writes rows to the target storage.
  - `buildInsertSelectPipeline()` – builds a pipeline that materialises the SELECT sub‑query and feeds its result blocks into the target storage.
- **Data flow:**
  1. Source processor reads raw input (socket, file, or generated rows).
  2. Optional `InputFormat` processor parses the format (CSV, JSONEachRow, …).
  3. `InsertBlockSink` processor writes blocks to the storage engine.

---

## 4. DROP TABLE Implementation

### AST Layer
- **Class:** `ASTDropQuery`
- **Enum `Kind` values:** `Drop`, `Detach`, `Truncate`.
- **Members:**
  - `bool if_exists`
  - `String database`, `String table`
  - `Kind kind`

### Parser Layer
- **Class:** `ParserDropQuery`
- **Handles:**
  - `DROP TABLE [IF EXISTS] <name>`
  - Optional `PERMANENTLY` keyword for hard delete.
  - `DETACH` and `TRUNCATE` as alternate kinds.

### Interpreter Layer
- **Class:** `InterpreterDropQuery`
- **Execution flow:**
  1. Acquire a **DDLGuard** (`DDLGuard guard(database, table)`), ensuring exclusive access.
  2. Resolve the target database via the context.
  3. Depending on `Kind`:
     - **Drop:** remove metadata and optionally delete files.
     - **Detach:** keep data on disk but unregister the table.
     - **Truncate:** clear all blocks while keeping the table definition.
  4. Update the **Database**'s internal map and persist changes.

---

## 5. ALTER TABLE Implementation

### AST Layer
- **Class:** `ASTAlterQuery`
- **Contains:** `ASTAlterCommandList * command_list`.
- **`ASTAlterCommand`** holds:
  - `AlterCommand::Type type` – e.g., `ADD_COLUMN`, `DROP_COLUMN`, `MODIFY_COLUMN`, `ADD_INDEX`, `DROP_INDEX`, `ATTACH_PARTITION`, `DROP_PARTITION`, … (30+ types).
  - Command‑specific fields such as column definition, index definition, partition expression, etc.

### Parser Layer
- **Class:** `ParserAlterQuery`
- **Parsing steps:**
  1. Consume `ALTER TABLE <name>`.
  2. Loop over comma‑separated `ALTER` commands, delegating each to `ParserAlterCommand`.
  3. Populate an `ASTAlterCommand` for each parsed token.

### Interpreter Layer
- **Class:** `InterpreterAlterQuery`
- **Dispatch:**
  - Iterates over `command_list` and calls the appropriate handler method (`addColumn()`, `dropColumn()`, `modifyColumn()`, `addIndex()`, `dropPartition()`, …).
  - Each handler validates the command, updates the **Storage** metadata (`StorageInMemoryMetadata`), and may rebuild parts of the storage engine (e.g., refresh column defaults, recompute indices).
- **Locking:** Uses **DDLGuard** similar to `DROP` to guarantee exclusive schema changes.

---

## 6. Key Patterns & Best Practices (ClickHouse → Mnemosyne)

| Pattern | ClickHouse Example | Mnemosyne Recommendation |
|---------|----------------------|--------------------------|
| **Storage Factory** | `StorageFactory::instance().get(name)` creates a concrete storage engine. | Introduce a **StorageFactory** registry to decouple engine creation from hard‑coded `switch` statements. |
| **DDL Guard / Locking** | `DDLGuard guard(database, table)` ensures exclusive schema modification. | Add a similar guard around `CREATE`, `DROP`, `ALTER` interpreter entry points to avoid race conditions in multi‑threaded workloads. |
| **Context‑based Execution** | `Context` carries settings, user, and database pointers. | Keep using the existing `Context` but expose helper methods for accessing the current database/metadata from interpreters. |
| **Metadata Management** | `StorageInMemoryMetadata` stores column definitions, defaults, and engine settings. | Ensure Mnemosyne's `TableMetadata` mirrors this structure; consider a versioned metadata object to simplify migrations. |
| **Transaction Support** | ClickHouse wraps DDL in a lightweight transaction‑like guard to allow rollback on errors. | Implement a **DDL transaction** that records the previous metadata state and restores it on failure. |
| **Visitor Pattern for AST** | `ASTVisitor` traverses nodes for analysis. | Adopt a visitor for static analysis (e.g., column‑type resolution) instead of manual recursion. |

---

## 7. Mnemosyne Relevance Section

- **Current state:** Mnemosyne already has a three‑layer flow (Lexer → Parser → Interpreter) but the **DDL interpreters** (`InterpreterCreateQuery`, `InterpreterInsertQuery`, etc.) are simple and lack the sophisticated guarding and factory patterns.
- **Gaps identified:**
  1. No dedicated **storage‑engine factory** – storage implementations are tightly coupled.
  2. Absence of a **DDLGuard** leads to potential race conditions when multiple queries modify schemas concurrently.
  3. Metadata is stored directly in the `Database` map without a versioned object, making schema migrations cumbersome.
- **Actionable recommendations:**
  1. Introduce a `StorageFactory` similar to ClickHouse and register existing storages (`Memory`, `File`).
  2. Add a lightweight `DDLGuard` class that acquires a mutex per database/table during DDL execution.
  3. Refactor `ASTCreateQuery`, `ASTAlterQuery`, and `ASTDropQuery` to use a common base `ASTDDLQuery` for shared fields (`if_exists`, `database`, `table`).
  4. Wrap DDL interpreter bodies in a try/catch that rolls back metadata on exception.
  5. Extend `Context` with helper `getDatabase(const String & name)` that returns a pointer or throws a descriptive error – this mirrors ClickHouse’s error handling.

---

## 8. Verification Checklist

- [ ] Document style matches `docs/ARCHITECTURE.md` (heading levels, code fences, ASCII diagrams).
- [ ] All class and method names (`ASTCreateQuery`, `ParserCreateQuery`, `InterpreterCreateQuery::doCreateTable`, etc.) are verified against the current source tree.
- [ ] Internal file links (if any) use relative paths like `../src/Parsers/ast.h`.
- [ ] No copyrighted ClickHouse source code is copied verbatim; only structural descriptions are provided.
- [ ] Spell‑check and grammar review completed.

---

*End of documentation.*
