# USE Statement Implementation Guide

This document provides a comprehensive guide for implementing the SQL `USE database_name` statement in Mnemosyne, based on how it's implemented in ClickHouse.

## Overview

The `USE` statement allows users to set the current database context for their session, so subsequent queries don't need to specify the database prefix for tables.

**Example:**
```sql
USE my_database;
SELECT * FROM users;  -- Uses my_database.users instead of needing my_database.users
```

## ClickHouse Implementation Analysis

### Architecture in ClickHouse

ClickHouse implements USE through three main components:

#### 1. AST Node (`src/Parsers/ASTUseQuery.h`)

```cpp
class ASTUseQuery : public IAST
{
public:
    IAST * database;  // Pointer to database identifier

    String getDatabase() const
    {
        String name;
        tryGetIdentifierNameInto(database, name);
        return name;
    }

    String getID(char delim) const override { 
        return "UseQuery" + (delim + getDatabase()); 
    }

    ASTPtr clone() const override
    {
        auto res = make_intrusive<ASTUseQuery>(*this);
        res->children.clear();
        if (database)
            res->set(res->database, database->clone());
        return res;
    }

    QueryKind getQueryKind() const override { return QueryKind::Use; }
};
```

**Key points:**
- Simple AST node with a single `database` field (IAST pointer)
- Has a helper method `getDatabase()` to extract the database name
- Implements standard AST methods (getID, clone, getQueryKind)

#### 2. Parser (`src/Parsers/ParserUseQuery.cpp`)

```cpp
bool ParserUseQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_use(Keyword::USE);
    ParserKeyword s_database(Keyword::DATABASE);
    ParserIdentifier name_p{/*allow_query_parameter*/ true};

    if (!s_use.ignore(pos, expected))
        return false;

    ASTPtr database;
    Expected test_expected;

    /// test if we have DATABASE <identifier> pattern without moving pos
    Pos test_pos = pos;
    ASTPtr test_node;

    bool has_database_keyword_pattern =
        s_database.parse(test_pos, test_node, test_expected) &&
        name_p.parse(test_pos, test_node, test_expected);

    // now the actual parsing
    if (has_database_keyword_pattern)
    {
        // Parse DATABASE <identifier>
        s_database.ignore(pos, expected);
        if (!name_p.parse(pos, database, expected))
            return false;
    }
    else
    {
        // Parse identifier directly (handles "USE database" where database is a name)
        if (!name_p.parse(pos, database, expected))
            return false;
    }

    auto query = make_intrusive<ASTUseQuery>();
    query->set(query->database, database);
    node = query;

    return true;
}
```

**Key points:**
- Handles both `USE database_name` and `USE DATABASE database_name` syntax
- First checks for the `DATABASE` keyword pattern, if found uses it
- Otherwise, parses the identifier directly
- Creates and populates the ASTUseQuery node

#### 3. Interpreter (`src/Interpreters/InterpreterUseQuery.cpp`)

```cpp
BlockIO InterpreterUseQuery::execute()
{
    const String & new_database = query_ptr->as<ASTUseQuery &>().getDatabase();
    getContext()->checkAccess(AccessType::SHOW_DATABASES, new_database);
    getContext()->getSessionContext()->setCurrentDatabase(new_database);
    return {};
}
```

**Key points:**
- Extracts the database name from the AST
- Checks access permissions (SHOW_DATABASES access type)
- Sets the current database in the **session context** (not query context)
- Returns empty BlockIO (no data output)

**Important:** ClickHouse distinguishes between query context and session context. USE affects the session context, so it persists across multiple queries in the same session.

## Mnemosyne Current State

### What's Already Implemented

1. **AST Node** (`src/Parsers/ast.h`, lines 60-67):
```cpp
class ASTUseDatabase final : public ASTNode {
public:
    std::string database_name;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTUseDatabase"; }
};
```

✅ The ASTUseDatabase class already exists with a `database_name` field.

2. **Lexer Token** (`src/Parsers/lexer.h`, line 29):
```cpp
KeywordDatabase, KeywordUse,
```

✅ The `KeywordUse` token is already defined in the lexer.

3. **Lexer Implementation** (`src/Parsers/lexer.cpp`, line 285):
```cpp
else if (upper == "USE") type = TokenType::KeywordUse;
```

✅ The lexer already recognizes the USE keyword.

4. **Context Methods** (`src/Interpreters/context.h`, lines 53-54):
```cpp
auto& current_database() { return current_db_; }
void  set_current_database(std::string name) { current_db_ = std::move(name); }
```

✅ The Context class already has methods to get/set the current database.

### What's Missing

1. **QueryType Enum** (`src/Parsers/ast.h`, line 219):
   - No `USE` type in the `QueryType` enum

2. **Parser** (`src/Parsers/parser.cpp`):
   - No `parse_use()` method
   - No USE case in `parse_query()` method (lines 25-56)

3. **QueryAST Use Struct** (`src/Parsers/ast.h`):
   - No `Use` struct defined in the QueryAST class

4. **Analyzer** (`src/Analyzer/analyzer.cpp`):
   - No USE case in the `analyze()` switch statement (lines 22-100)

5. **Execution Plan Node Type** (`src/Planner/execution_plan.h`, lines 40-53):
   - No `USE` type in the `PlanNode::Type` enum

6. **Planner** (`src/Planner/planner.cpp`):
   - No USE handling in the `plan()` method (lines 25-113)

7. **Interpreter** (`src/Interpreters/interpreter.cpp`):
   - No USE case in the `create_processor()` switch statement (lines 66-134)

8. **Processors** (`src/Processors/processors.h` and `processors_source.cpp`):
   - No `UseProcessor` class defined

## Implementation Plan

Following the pattern used by CREATE DATABASE in Mnemosyne, here are the step-by-step changes needed:

### Step 1: Update QueryType Enum

**File:** `src/Parsers/ast.h` (line 219)

Add `USE` to the QueryType enum:

```cpp
enum class QueryType { SELECT, INSERT, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN, USE };
```

### Step 2: Add Use Struct to QueryAST

**File:** `src/Parsers/ast.h` (after line 264, before the closing of QueryAST class)

Add a Use struct to hold the database name:

```cpp
struct Use {
    std::string database_name;
} use;
```

### Step 3: Add parse_use() Method

**File:** `src/Parsers/parser.cpp` (add new method after `parse_explain()`)

```cpp
void Parser::parse_use(std::unique_ptr<QueryAST>& ast) {
    auto& use = ast->use;

    consume(); // consume USE
    
    // Check for optional DATABASE keyword (matches ClickHouse behavior)
    if (current_.type == TokenType::KeywordDatabase) {
        consume(); // consume DATABASE
    }
    
    use.database_name = parse_table_name();
}
```

### Step 4: Add USE Case in parse_query()

**File:** `src/Parsers/parser.cpp` (add case at line 48, before the else)

```cpp
else if (current_.type == TokenType::KeywordUse) {
    ast->query_type = QueryAST::QueryType::USE;
    parse_use(ast);
}
```

### Step 5: Add USE Case in Analyzer

**File:** `src/Analyzer/analyzer.cpp` (add case at line 99, before default)

```cpp
case parsers::QueryAST::QueryType::USE: {
    result.analyzed_ast = ast;
    result.valid = true;
    break;
}
```

**Note:** Like CREATE DATABASE, USE is a simple statement that doesn't require semantic analysis beyond parsing.

### Step 6: Add USE to Execution Plan Node Type

**File:** `src/Planner/execution_plan.h` (line 52, after EXPLAIN)

```cpp
enum class Type : uint8_t {
    SCAN,       // table scan
    FILTER,     // WHERE predicate
    PROJECT,    // SELECT columns
    GROUP_BY,   // GROUP BY aggregation
    SORT,       // ORDER BY
    LIMIT,      // LIMIT / OFFSET
    INSERT,     // INSERT INTO
    CREATE,     // CREATE TABLE
    DROP,       // DROP TABLE
    SHOW,       // SHOW TABLES / DATABASES
    DESCRIBE,   // DESCRIBE TABLE
    EXPLAIN,    // EXPLAIN PLAN
    USE,        // USE DATABASE
};
```

### Step 7: Add USE Handling in Planner

**File:** `src/Planner/planner.cpp` (add case after line 109, before default)

Add USE handling in the plan() method. You have two options:

**Option A: Follow CREATE DATABASE pattern (recommended)**
```cpp
} else if (type == "UseDatabase") {
    auto* use = dynamic_cast<analyzer::UseDatabaseNode*>(tree.get());
    if (use) {
        auto node = std::make_shared<PlanNode>();
        node->node_type = PlanNode::Type::USE;
        node->name = use->database_name;  // Store database name in name field
        plan->root = node;
    }
```

**Option B: Handle USE in planner without new query tree node (simpler)**
Since USE doesn't need a complex query tree, you could handle it differently in the planner, but following the pattern is more consistent.

### Step 8: Add USE Case in Interpreter

**File:** `src/Interpreters/interpreter.cpp` (add case at line 129, before default)

**Option A: UseProcessor approach (follows CREATE pattern)**
```cpp
case PlanNode::Type::USE: {
    auto use_proc = std::make_shared<processors::UseProcessor>(
        root->name,  // database_name stored in name field
        *context);
    return use_proc;
}
```

**Option B: Handle directly in interpreter (simpler)**
```cpp
case PlanNode::Type::USE: {
    // Set current database directly in context
    context_->set_current_database(root->name);
    // Return a simple processor that produces an empty result
    auto use_proc = std::make_shared<processors::EmptyBlockSource>();
    return use_proc;
}
```

**Recommendation:** Use Option A for consistency with other DDL statements, but Option B is simpler and avoids creating a new processor class.

### Step 9: Add UseProcessor (if using Option A)

**File:** `src/Processors/processors.h` (add after CreateProcessor)

```cpp
// ── UseProcessor — sets the current database for the session ──
class UseProcessor final : public Processor {
public:
    UseProcessor(std::string database_name, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string database_name_;
    interpreters::Context& context_;
    core::Block result_data_;
    bool finished_ = false;
};
```

**File:** `src/Processors/processors_source.cpp` (add implementation)

```cpp
// ── UseProcessor ──

UseProcessor::UseProcessor(std::string database_name, interpreters::Context& context)
    : database_name_(std::move(database_name)),
      context_(context),
      finished_(false) {}

auto UseProcessor::is_finished() const -> bool { return finished_; }

auto UseProcessor::getHeader() const -> core::Block { return create_empty_block(); }

void UseProcessor::start() {
    // Set the current database in context
    context_.set_current_database(database_name_);
    
    // Return empty result (USE doesn't produce data)
    result_data_ = create_empty_block();
    finished_ = true;
}

auto UseProcessor::result() const -> std::optional<core::Block> {
    if (!finished_) return std::nullopt;
    return result_data_;
}
```

### Step 10: Update Grammar Documentation

**File:** `docs/GRAMMAR.md`

Add USE to the grammar (line 12-13):
```diff
<statement>       → <select_statement> | <insert_statement> | <update_statement>
                   | <delete_statement> | <create_database_statement>
                   | <create_table_statement> | <drop_table_statement>
                   | <alter_table_statement> | <drop_database_statement>
+                  | <use_statement> | <set_statement> | <explain_statement>
```

Add USE statement definition (after line 49):
```
<use_statement>       → USE <name>
```

Add USE to reserved keywords (line 72):
```diff
- SELECT, FROM, WHERE, GROUP, BY, HAVING, ORDER, LIMIT, ALL, DISTINCT
- INSERT, INTO, VALUES, UPDATE, SET, DELETE
- CREATE, DATABASE, TABLE, DROP, ALTER, ADD, COLUMN
+ SELECT, FROM, WHERE, GROUP, BY, HAVING, ORDER, LIMIT, ALL, DISTINCT
+ INSERT, INTO, VALUES, UPDATE, SET, DELETE
+ CREATE, DATABASE, TABLE, DROP, ALTER, ADD, COLUMN, USE
```

## Key Differences Between ClickHouse and Mnemosyne

### Session vs Query Context

**ClickHouse:**
- Has separate query context and session context
- USE affects session context (`getSessionContext()->setCurrentDatabase()`)
- Database change persists across multiple queries in the session

**Mnemosyne:**
- Currently appears to have a single Context class
- USE would affect `context_.current_database_`
- This may need to be extended to support session-level state if multiple queries are run in sequence

### Implementation Approach

**ClickHouse:**
- Dedicated interpreter that directly sets session context
- Returns empty BlockIO (no processor needed)
- Access control checks performed in interpreter

**Mnemosyne:**
- Follows processor pattern for all operations
- Should decide whether to create a UseProcessor or handle directly in interpreter
- May want to add database existence check before setting

## Testing Recommendations

### Unit Tests

Add tests to verify:

1. **Basic USE functionality:**
```cpp
TEST_F(Level1DDLTest, UseDatabaseSQL) {
    // Create a database
    auto db = db_manager.create_database("my_db");
    
    // Execute USE statement
    auto result = interpreter.execute("USE my_db");
    
    // Verify current database is set
    EXPECT_EQ(context_.current_database(), "my_db");
}
```

2. **USE with DATABASE keyword:**
```cpp
TEST_F(Level1DDLTest, UseDatabaseWithKeyword) {
    auto db = db_manager.create_database("my_db");
    auto result = interpreter.execute("USE DATABASE my_db");
    EXPECT_EQ(context_.current_database(), "my_db");
}
```

3. **USE non-existent database:**
```cpp
TEST_F(Level1DDLTest, UseDatabaseNonExistent) {
    // Should throw or return error
    EXPECT_THROW(
        interpreter.execute("USE non_existent_db"),
        common::Exception
    );
}
```

4. **USE affects subsequent queries:**
```cpp
TEST_F(Level1DDLTest, UseDatabaseAffectsSubsequentQueries) {
    auto db = db_manager.create_database("test_db");
    context_.register_database("test_db", db);
    
    // Create table in test_db
    std::unordered_map<std::string, datatypes::DataTypePtr> columns = {
        {"id", datatypes::get_data_type("Int64")}
    };
    db->create_table("test_table", columns, "Memory");
    
    // Use the database
    interpreter.execute("USE test_db");
    
    // Query should use the current database
    auto result = interpreter.execute("SELECT * FROM test_table");
    // Verify table from current database is used
}
```

## Summary

To implement the USE statement in Mnemosyne:

1. ✅ **Already done:** AST node, lexer token, context methods
2. **Need to add:** QueryType enum value, QueryAST struct, parser method, analyzer case, plan node type, planner handling, interpreter case, optionally UseProcessor
3. **Testing:** Add unit tests for basic USE, USE with keyword, error cases, and session persistence

The implementation follows the established patterns in Mnemosyne for DDL statements, particularly CREATE DATABASE. The main architectural consideration is whether to create a dedicated UseProcessor or handle USE directly in the interpreter since it's a simple context-setting operation.

## References

- ClickHouse ASTUseQuery: `/c:/Users/tsuma.thomas/Documents/ClickHouse/src/Parsers/ASTUseQuery.h`
- ClickHouse ParserUseQuery: `/c:/Users/tsuma.thomas/Documents/ClickHouse/src/Parsers/ParserUseQuery.cpp`
- ClickHouse InterpreterUseQuery: `/c:/Users/tsuma.thomas/Documents/ClickHouse/src/Interpreters/InterpreterUseQuery.cpp`
- Mnemosyne AST: `C:/Users/tsuma.thomas/Documents/Mnemosyne/src/Parsers/ast.h`
- Mnemosyne Parser: `C:/Users/tsuma.thomas/Documents/Mnemosyne/src/Parsers/parser.cpp`
- Mnemosyne Context: `C:/Users/tsuma.thomas/Documents/Mnemosyne/src/Interpreters/context.h`