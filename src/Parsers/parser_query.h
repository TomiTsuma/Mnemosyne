// src/Parsers/parser_query.h — SQL query parser (SELECT, INSERT, CREATE, etc.)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "parser.h"
#include "ast.h"
#include <memory>

namespace mnesso::parsers {

// ── QueryParser — parses SQL SELECT queries into ASTSelectQuery ──
class QueryParser final : public Parser {
public:
    using Parser::Parser;

    auto parse() -> std::variant<std::shared_ptr<ASTNode>, ParseError> override;

    // ── Grammar rules ──
    auto parse_select_query() -> std::shared_ptr<ASTSelectQuery>;
    auto parse_select_list()  -> std::vector<std::shared_ptr<ASTExpr>>;
    auto parse_from_clause()  -> std::shared_ptr<ASTFromClause>;
    auto parse_where_clause() -> std::shared_ptr<ASTExpr>;
    auto parse_group_by()     -> std::vector<std::shared_ptr<ASTExpr>>;
    auto parse_having()       -> std::shared_ptr<ASTExpr>;
    auto parse_order_by()     -> std::vector<std::pair<std::shared_ptr<ASTExpr>, bool>>;
    auto parse_limit()        -> std::pair<size_t, std::optional<size_t>>;

    // ── Expression parsing ──
    auto parse_expression() -> std::shared_ptr<ASTExpr>;
    auto parse_primary()    -> std::shared_ptr<ASTExpr>;
    auto parse_unary()      -> std::shared_ptr<ASTExpr>;
    auto parse_factor()     -> std::shared_ptr<ASTExpr>;
    auto parse_term()       -> std::shared_ptr<ASTExpr>;
    auto parse_comparison() -> std::shared_ptr<ASTExpr>;

    // ── Other query types ──
    auto parse_create_table() -> std::shared_ptr<ASTCreateTable>;
    auto parse_insert()       -> std::shared_ptr<ASTInsertQuery>;

private:
    // Token matching helpers
    auto matches(TokenType type) const -> bool;
    auto consume(TokenType type) -> std::optional<Token>;
};

} // namespace mnesso::parsers
