// tests/test_analyzer.h — Unit tests for query analyzer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "analyzer/analyzer.h"
#include "parsers/ast.h"
#include "parsers/lexer.h"
#include "parsers/parser_query.h"
#include <catch2/catch_all.hpp>

// ── Analyzer tests ──
TEST_CASE("Analyzer resolves column references", "[analyzer]") {
    mnesso::interpreters::Context context;
    mnesso::analyzer::Analyzer analyzer(context);

    // Create a mock parsed AST
    mnesso::parsers::Lexer lexer("SELECT a FROM t WHERE a > 5");
    mnesso::parsers::QueryParser parser(lexer);
    auto result = parser.parse();

    REQUIRE(std::holds_alternative<std::shared_ptr<mnesso::parsers::ASTNode>>(result));
    auto& ast = std::get<std::shared_ptr<mnesso::parsers::ASTNode>>(result);

    auto analysis = analyzer.analyze(ast);
    // TODO: verify analysis results
}

TEST_CASE("Analyzer validates function arguments", "[analyzer]") {
    mnesso::interpreters::Context context;
    mnesso::analyzer::Analyzer analyzer(context);

    // TODO: test function argument validation
}
