# Grammar Reference

This section documents the Mnemo SQL language at the syntactic level. The **authoritative implementation** is the recursive-descent parser in `src/Parsers/parser.cpp` and the lexer in `src/Parsers/lexer.cpp`. The ANTLR-style stub in `src/Parsers/grammar.y` describes planned/full syntax.

## Files

- [full-grammar.md](full-grammar.md) — EBNF by nonterminal
- [keywords.md](keywords.md) — reserved words and lexer tokens
- [statement-catalog.md](statement-catalog.md) — every top-level statement with examples

## Parser pipeline

```
SQL text
  → Lexer (tokens)
  → Parser (QueryAST)
  → Analyzer
  → Interpreter / Executor
```

## QueryAST types

Top-level `QueryAST::QueryType` enum:

`SELECT`, `INSERT`, `CREATE`, `DROP`, `ALTER`, `SHOW`, `DESCRIBE`, `EXPLAIN`, `USE`, `REFRESH`, `REGISTER`, `DRAIN`, `REMOVE`, `TEST`, `DISCOVER`, `RUN`, `PAUSE`, `RESUME`, `PUBLISH`, `SUBSCRIBE`, `DEPLOY`, `PREDICT`, `EVALUATE`, `COMPARE`, `GENERATE`

## Expression AST

- `ASTLiteral` — int, float, string, bool, null
- `ASTColumnRef` — column or `table.column`
- `ASTFunction` — calls and aggregates; optional `OVER` window
- `ASTBinaryOp` — arithmetic, comparison, `AND`/`OR`, `IN`
- `ASTUnaryOp` — negation, not
- `ASTAlias` — expression with alias
- `ASTSubQueryExpr` — subquery in `IN` or scalar context

## Compatibility notes

Mnemo SQL intentionally resembles ClickHouse/SQL analytics dialects for:

- Type names (`Int64`, `String`, `DateTime`)
- `ENGINE =` table clause
- `USE DATABASE`

Entity and model extensions are Mnemo-specific.

## Contributing grammar changes

1. Update `lexer.cpp` keywords if needed
2. Extend `parser.cpp` parse methods
3. Extend `ast.h` structures
4. Add integration test in `scripts/test_*_api.py`
5. Update this documentation and `full-grammar.md`
