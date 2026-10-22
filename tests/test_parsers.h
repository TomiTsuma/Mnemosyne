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

// ── SELECT expression grammar ──

namespace mnemo::tests {

inline auto parse_sql(const std::string& sql) -> std::unique_ptr<mnemo::parsers::QueryAST> {
    mnemo::parsers::Lexer lexer(sql);
    mnemo::parsers::QueryParser parser(lexer);
    return parser.parse();
}

} // namespace mnemo::tests

TEST_CASE("Parser binds AND tighter than OR", "[parser][expr]") {
    auto ast = mnemo::tests::parse_sql("SELECT a FROM t WHERE a = 1 OR b = 2 AND c = 3");
    REQUIRE(ast != nullptr);

    auto* top = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(ast->select.where.get());
    REQUIRE(top != nullptr);
    REQUIRE(top->op == mnemo::parsers::ASTBinaryOp::Op::Or);
    auto* right = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(top->right.get());
    REQUIRE(right != nullptr);
    REQUIRE(right->op == mnemo::parsers::ASTBinaryOp::Op::And);
}

TEST_CASE("Parser binds * tighter than + and maps % to Mod", "[parser][expr]") {
    auto ast = mnemo::tests::parse_sql("SELECT 2 + 3 * 4, a % 5 FROM t");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->select.columns.size() == 2);

    auto* sum = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(ast->select.columns[0].get());
    REQUIRE(sum != nullptr);
    REQUIRE(sum->op == mnemo::parsers::ASTBinaryOp::Op::Add);
    auto* product = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(sum->right.get());
    REQUIRE(product != nullptr);
    REQUIRE(product->op == mnemo::parsers::ASTBinaryOp::Op::Mul);

    auto* modulo = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(ast->select.columns[1].get());
    REQUIRE(modulo != nullptr);
    REQUIRE(modulo->op == mnemo::parsers::ASTBinaryOp::Op::Mod);
}

TEST_CASE("Parser attaches an alias to the whole select item", "[parser][alias]") {
    auto ast = mnemo::tests::parse_sql("SELECT age > 25 AS older, name n, SUM(x) / COUNT(*) AS avg_x FROM t");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->select.columns.size() == 3);

    auto* older = dynamic_cast<mnemo::parsers::ASTAlias*>(ast->select.columns[0].get());
    REQUIRE(older != nullptr);
    REQUIRE(older->alias == "older");
    auto* comparison = dynamic_cast<mnemo::parsers::ASTBinaryOp*>(older->expression.get());
    REQUIRE(comparison != nullptr);
    REQUIRE(comparison->op == mnemo::parsers::ASTBinaryOp::Op::Gt);

    auto* bare = dynamic_cast<mnemo::parsers::ASTAlias*>(ast->select.columns[1].get());
    REQUIRE(bare != nullptr);
    REQUIRE(bare->alias == "n");

    auto* ratio = dynamic_cast<mnemo::parsers::ASTAlias*>(ast->select.columns[2].get());
    REQUIRE(ratio != nullptr);
    REQUIRE(dynamic_cast<mnemo::parsers::ASTBinaryOp*>(ratio->expression.get()) != nullptr);
}

TEST_CASE("Parser accepts a keyword as an alias after AS", "[parser][alias]") {
    auto ast = mnemo::tests::parse_sql("SELECT NTILE(4) OVER (ORDER BY age) AS bucket FROM t");
    REQUIRE(ast != nullptr);
    auto* alias = dynamic_cast<mnemo::parsers::ASTAlias*>(ast->select.columns[0].get());
    REQUIRE(alias != nullptr);
    REQUIRE(alias->alias == "bucket");
}

TEST_CASE("Parser folds unary minus into numeric literals", "[parser][expr]") {
    auto ast = mnemo::tests::parse_sql("SELECT -5, -2.5, -a FROM t");
    REQUIRE(ast != nullptr);

    auto* integer = dynamic_cast<mnemo::parsers::ASTLiteral*>(ast->select.columns[0].get());
    REQUIRE(integer != nullptr);
    REQUIRE(std::get<int64_t>(integer->value) == -5);
    auto* real = dynamic_cast<mnemo::parsers::ASTLiteral*>(ast->select.columns[1].get());
    REQUIRE(real != nullptr);
    REQUIRE(std::get<double>(real->value) == -2.5);
    auto* negated = dynamic_cast<mnemo::parsers::ASTUnaryOp*>(ast->select.columns[2].get());
    REQUIRE(negated != nullptr);
    REQUIRE(negated->op == mnemo::parsers::ASTUnaryOp::Op::Neg);
}

TEST_CASE("Parser reads LIMIT in all three forms", "[parser][limit]") {
    auto plain = mnemo::tests::parse_sql("SELECT a FROM t LIMIT 3");
    REQUIRE(plain != nullptr);
    REQUIRE(plain->select.has_limit);
    REQUIRE(plain->select.limit == std::pair<size_t, size_t>{0, 3});

    auto with_offset = mnemo::tests::parse_sql("SELECT a FROM t LIMIT 3 OFFSET 2");
    REQUIRE(with_offset != nullptr);
    REQUIRE(with_offset->select.limit == std::pair<size_t, size_t>{2, 3});

    auto comma = mnemo::tests::parse_sql("SELECT a FROM t LIMIT 2, 3");
    REQUIRE(comma != nullptr);
    REQUIRE(comma->select.limit == std::pair<size_t, size_t>{2, 3});

    auto zero = mnemo::tests::parse_sql("SELECT a FROM t LIMIT 0");
    REQUIRE(zero != nullptr);
    REQUIRE(zero->select.has_limit);
    REQUIRE(zero->select.limit.second == 0);

    auto none = mnemo::tests::parse_sql("SELECT a FROM t");
    REQUIRE(none != nullptr);
    REQUIRE_FALSE(none->select.has_limit);
}

TEST_CASE("Parser reads parametric calls like quantile(0.9)(x)", "[parser][expr]") {
    auto ast = mnemo::tests::parse_sql("SELECT quantile(0.9)(age), median(age) FROM t");
    REQUIRE(ast != nullptr);

    auto* quantile = dynamic_cast<mnemo::parsers::ASTFunction*>(ast->select.columns[0].get());
    REQUIRE(quantile != nullptr);
    REQUIRE(quantile->parameters.size() == 1);
    REQUIRE(quantile->args.size() == 1);
    REQUIRE(quantile->is_aggregate);

    auto* median = dynamic_cast<mnemo::parsers::ASTFunction*>(ast->select.columns[1].get());
    REQUIRE(median != nullptr);
    REQUIRE(median->parameters.empty());
    REQUIRE(median->is_aggregate);
}

TEST_CASE("Parser reads a qualified ORDER BY column", "[parser][order]") {
    auto ast = mnemo::tests::parse_sql("SELECT a FROM t x ORDER BY x.a DESC, b");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->select.order_by.size() == 2);
    REQUIRE(ast->select.order_by[0].first == "x.a");
    REQUIRE(ast->select.order_by[0].second);
    REQUIRE(ast->select.order_by[1].first == "b");
}

TEST_CASE("Parser rejects tokens left over after SELECT", "[parser][errors]") {
    REQUIRE_THROWS(mnemo::tests::parse_sql("SELECT a FROM t WHERE a > 1 BANANA"));
    REQUIRE_THROWS(mnemo::tests::parse_sql("SELECT a FROM t ORDER BY a * 2"));
    REQUIRE_THROWS(mnemo::tests::parse_sql("SELECT a FROM t LIMIT x"));
}

// ── CREATE TRIGGER: event trigger grammar ──
// A trigger with no event clause is manual; otherwise exactly one of SCHEDULE / ON INSERT INTO /
// ON STREAM / WEBHOOK is set. The interpreter, not the parser, checks that the table or stream
// exists.

TEST_CASE("Parser parses a manual trigger", "[parser][trigger]") {
    auto ast = mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->query_type == mnemo::parsers::QueryAST::QueryType::CREATE);
    REQUIRE(ast->create.kind == mnemo::parsers::QueryAST::Create::Kind::Trigger);
    REQUIRE(ast->create.trigger_name == "t");
    REQUIRE(ast->create.pipeline_name == "p");
    REQUIRE(ast->create.trigger_schedule.empty());
    REQUIRE(ast->create.trigger_table.empty());
    REQUIRE(ast->create.trigger_stream.empty());
    REQUIRE_FALSE(ast->create.trigger_webhook);
}

TEST_CASE("Parser parses a schedule trigger", "[parser][trigger]") {
    auto ast = mnemo::tests::parse_sql("CREATE TRIGGER nightly ON PIPELINE p SCHEDULE '0 0 * * *'");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->create.kind == mnemo::parsers::QueryAST::Create::Kind::Trigger);
    REQUIRE(ast->create.trigger_schedule == "0 0 * * *");
    REQUIRE(ast->create.trigger_table.empty());
}

TEST_CASE("Parser parses a table-insert trigger", "[parser][trigger]") {
    SECTION("unqualified table") {
        auto ast = mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p ON INSERT INTO orders");
        REQUIRE(ast->create.trigger_table == "orders");
        REQUIRE(ast->create.trigger_stream.empty());
    }
    SECTION("qualified table keeps the database") {
        auto ast = mnemo::tests::parse_sql(
            "CREATE TRIGGER t ON PIPELINE p ON INSERT INTO sales.orders");
        REQUIRE(ast->create.trigger_table == "sales.orders");
    }
}

TEST_CASE("Parser parses a stream trigger", "[parser][trigger]") {
    auto ast = mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p ON STREAM clicks");
    REQUIRE(ast->create.trigger_stream == "clicks");
    REQUIRE(ast->create.trigger_table.empty());
    REQUIRE_FALSE(ast->create.trigger_webhook);
}

TEST_CASE("Parser parses a webhook trigger", "[parser][trigger]") {
    SECTION("without a secret") {
        auto ast = mnemo::tests::parse_sql("CREATE TRIGGER hook ON PIPELINE p WEBHOOK");
        REQUIRE(ast->create.trigger_webhook);
        REQUIRE(ast->create.trigger_secret.empty());
    }
    SECTION("with a secret") {
        auto ast = mnemo::tests::parse_sql(
            "CREATE TRIGGER hook ON PIPELINE p WEBHOOK SECRET 'my-token'");
        REQUIRE(ast->create.trigger_webhook);
        REQUIRE(ast->create.trigger_secret == "my-token");
    }
}

TEST_CASE("Parser rejects malformed trigger clauses", "[parser][trigger][errors]") {
    // ON INSERT without INTO.
    REQUIRE_THROWS(mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p ON INSERT orders"));
    // ON followed by neither INSERT INTO nor STREAM.
    REQUIRE_THROWS(mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p ON orders"));
    // WEBHOOK SECRET without a quoted token.
    REQUIRE_THROWS(mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p WEBHOOK SECRET"));
    // A mistyped event keyword must not be silently ignored as a manual trigger.
    REQUIRE_THROWS(mnemo::tests::parse_sql("CREATE TRIGGER t ON PIPELINE p EVERY '5m'"));
}
