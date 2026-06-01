// tests/test_analyzer.h — Unit tests for query analyzer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

// Enable default-constructible Context for tests (must come before any include
// that transitively pulls in context.h)
#define ALLOW_CONTEXT_DEFAULT_CTOR

#include "analyzer/analyzer.h"
#include "parsers/ast.h"
#include "parsers/lexer.h"
#include "parsers/parser_query.h"
#include <catch2/catch_all.hpp>

// ── Helper: build a minimal Context for tests ──
static mnesso::interpreters::Context make_test_context() {
    // Context requires a database; for analyzer unit tests we only need the
    // settings and logger to be non-null (the analyzer doesn't call get_storage
    // in its basic analyze() path).  Use the production constructor with a
    // nullptr — the Context will be default-constructed below, which is safe
    // because ALLOW_CONTEXT_DEFAULT_CTOR is defined.
    return mnesso::interpreters::Context{};
}

// ── Analyzer tests ──
TEST_CASE("Analyzer resolves column references", "[analyzer]") {
    auto context = make_test_context();
    mnesso::analyzer::Analyzer analyzer(context);

    // Parse a query — parse() returns std::unique_ptr<QueryAST>, NOT a variant
    mnesso::parsers::Lexer lexer("SELECT a FROM t WHERE a > 5");
    mnesso::parsers::QueryParser parser(lexer);
    auto parsed = parser.parse();

    // QueryAST derives from ASTNode, so we can use it as the analyzed AST
    REQUIRE(parsed != nullptr);
    // Convert unique_ptr → shared_ptr (Analyzer::analyze takes shared_ptr)
    auto ast = std::shared_ptr<mnesso::parsers::QueryAST>(
        parsed.release());

    auto analysis = analyzer.analyze(ast);
    // TODO: verify analysis results
}

TEST_CASE("Analyzer validates function arguments", "[analyzer]") {
    auto context = make_test_context();
    mnesso::analyzer::Analyzer analyzer(context);

    // TODO: test function argument validation
}
