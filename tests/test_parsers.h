// tests/test_parsers.h — Unit tests for SQL parser
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "parsers/lexer.h"
#include "parsers/parser_query.h"
#include "parsers/ast.h"
#include <catch2/catch_all.hpp>

// // ── Lexer tests ──
// TEST_CASE("Lexer tokenizes simple SELECT", "[lexer]") {
//     mnemo::parsers::Lexer lexer("SELECT 1");
//     auto tok = lexer.next();
//     REQUIRE(tok.type == mnemo::parsers::TokenType::SELECT);
// }

// TEST_CASE("Lexer tokenizes literals", "[lexer]") {
//     mnemo::parsers::Lexer lexer("SELECT 42, 3.14, 'hello'");
//     auto t1 = lexer.next();
//     auto t2 = lexer.next();
//     auto t3 = lexer.next();
//     auto t4 = lexer.next();

//     REQUIRE(t1.type == mnemo::parsers::TokenType::SELECT);
//     REQUIRE(t2.type == mnemo::parsers::TokenType::INTEGER_LITERAL);
//     REQUIRE(t3.type == mnemo::parsers::TokenType::FLOAT_LITERAL);
//     REQUIRE(t4.type == mnemo::parsers::TokenType::STRING_LITERAL);
// }

// ── Parser tests ──
// TEST_CASE("Parser parses SELECT * FROM table", "[parser]") {
//     mnemo::parsers::Lexer lexer("SELECT * FROM table1");
//     mnemo::parsers::QueryParser parser(lexer);
//     auto result = parser.parse();

//     REQUIRE(std::holds_alternative<std::shared_ptr<mnemo::parsers::ASTNode>>(result));
//     auto& ast = std::get<std::shared_ptr<mnemo::parsers::ASTNode>>(result);
//     REQUIRE(ast != nullptr);
// }

TEST_CASE("Parser parses WHERE clause with comparison", "[parser]") {
    mnemo::parsers::Lexer lexer("SELECT a FROM t WHERE a > 5");
    mnemo::parsers::QueryParser parser(lexer);
    auto result = parser.parse();

    REQUIRE(result != nullptr);
}

TEST_CASE("Parser parses WHERE clause with AND/OR comparisons", "[parser]") {
    mnemo::parsers::Lexer lexer("SELECT a FROM t WHERE a > 5 AND b < 10 OR c = 3");
    mnemo::parsers::QueryParser parser(lexer);
    auto result = parser.parse();

    REQUIRE(result != nullptr);
}
