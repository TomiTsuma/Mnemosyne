// src/Parsers/parser.cpp — SQL parser for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Parsers/parser.h"
#include "Parsers/parser_query.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cmath>

namespace mnesso::parsers {

// ── Parser ──

Parser::Parser(Lexer lexer) : lexer_{lexer}, current_{lexer_.next()} {}

auto Parser::parse() -> std::unique_ptr<QueryAST> {
    current_ = lexer_.next();
    return parse_query();
}

auto Parser::parse_query() -> std::unique_ptr<QueryAST> {
    auto ast = std::make_unique<QueryAST>();

    if (current_.type == TokenType::KeywordSelect) {
        ast->query_type = QueryAST::QueryType::SELECT;
        parse_select(ast);
    } else if (current_.type == TokenType::KeywordInsert) {
        ast->query_type = QueryAST::QueryType::INSERT;
        parse_insert(ast);
    } else if (current_.type == TokenType::KeywordCreate) {
        ast->query_type = QueryAST::QueryType::CREATE;
        parse_create(ast);
    } else if (current_.type == TokenType::KeywordDrop) {
        ast->query_type = QueryAST::QueryType::DROP;
        parse_drop(ast);
    } else if (current_.type == TokenType::KeywordShow) {
        ast->query_type = QueryAST::QueryType::SHOW;
        parse_show(ast);
    } else if (current_.type == TokenType::KeywordDescribe || current_.type == TokenType::KeywordDesc) {
        ast->query_type = QueryAST::QueryType::DESCRIBE;
        parse_describe(ast);
    } else if (current_.type == TokenType::KeywordExplain) {
        ast->query_type = QueryAST::QueryType::EXPLAIN;
        parse_explain(ast);
    } else {
        std::string msg = "Parser: unexpected token '" + current_.value + "'";
        int code = static_cast<int>(common::ErrorCode::SYNTAX_ERROR);
        throw common::Exception{std::move(msg), code};
    }

    return ast;
}

void Parser::parse_select(std::unique_ptr<QueryAST>& ast) {
    auto& select = ast->select;

    // SELECT <columns> FROM <table>
    consume(); // consume SELECT
    select.columns = parse_expression_list();

    if (current_.type == TokenType::KeywordFrom) {
        consume(); // consume FROM
        select.table = parse_table_name();
    }

    if (current_.type == TokenType::KeywordWhere) {
        consume(); // consume WHERE
        select.where = parse_expression();
    }

    if (current_.type == TokenType::KeywordGroup) {
        consume(); // consume GROUP
        if (current_.type == TokenType::KeywordBy) {
            consume(); // consume BY
            select.group_by = parse_expression_list();
        }
    }

    if (current_.type == TokenType::KeywordHaving) {
        consume(); // consume HAVING
        select.having = parse_expression();
    }

    if (current_.type == TokenType::KeywordOrder) {
        consume(); // consume ORDER
        if (current_.type == TokenType::KeywordBy) {
            consume(); // consume BY
            auto orders = parse_order_by_list();
            for (auto& ob : orders) {
                select.order_by.emplace_back(ob.column,
                    ob.direction == OrderBy::Direction::DESC);
            }
        }
    }

    if (current_.type == TokenType::KeywordLimit) {
        consume(); // consume LIMIT
        select.limit = parse_limit();
    }
}

void Parser::parse_insert(std::unique_ptr<QueryAST>& ast) {
    auto& insert = ast->insert;

    consume(); // consume INSERT
    if (current_.type != TokenType::KeywordInto) {
        throw common::Exception{
            "Parser: expected INTO after INSERT",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume INTO
    insert.table = parse_table_name();
    insert.columns = parse_column_list();
    if (current_.type == TokenType::KeywordValues) {
        consume(); // consume VALUES
        insert.values = parse_value_list();
    }
}

void Parser::parse_create(std::unique_ptr<QueryAST>& ast) {
    auto& create = ast->create;

    consume(); // consume CREATE
    if (current_.type == TokenType::KeywordDatabase) {
        consume(); // consume DATABASE
        create.database_name = parse_table_name();
    } else if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLE
        create.table_name = parse_table_name();
        create.columns = parse_column_definitions();
    }
}

void Parser::parse_drop(std::unique_ptr<QueryAST>& ast) {
    auto& drop = ast->drop;

    consume(); // consume DROP
    if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLE
        drop.table_name = parse_table_name();
    }
}

void Parser::parse_show(std::unique_ptr<QueryAST>& ast) {
    auto& show = ast->show;

    consume(); // consume SHOW
    if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLES
        show.show_type = QueryAST::Show::ShowType::TABLES;
    } else {
        show.show_type = QueryAST::Show::ShowType::DATABASES;
    }
}

void Parser::parse_describe(std::unique_ptr<QueryAST>& ast) {
    auto& describe = ast->describe;

    consume(); // consume DESCRIBE/DESC
    describe.table_name = parse_table_name();
}

void Parser::parse_explain(std::unique_ptr<QueryAST>& ast) {
    consume(); // consume EXPLAIN
    // Parse the query to be explained
    ast->explain.explain_query = parse_query();
}

// ── Expression parsing ──

auto Parser::parse_expression() -> std::shared_ptr<ASTExpr> {
    auto left = parse_term();

    while (current_.type == TokenType::KeywordAnd ||
           current_.type == TokenType::KeywordOr) {
        auto op = std::make_shared<ASTBinaryOp>();
        op->op = current_.type == TokenType::KeywordAnd ?
                  ASTBinaryOp::Op::And : ASTBinaryOp::Op::Or;
        consume();
        auto right = parse_term();
        op->left = left;
        op->right = right;
        left = op;
    }

    return left;
}

auto Parser::parse_term() -> std::shared_ptr<ASTExpr> {
    auto left = parse_factor();

    while (current_.type == TokenType::Plus ||
           current_.type == TokenType::Minus ||
           current_.type == TokenType::Star ||
           current_.type == TokenType::Slash ||
           current_.type == TokenType::Percent) {
        auto op = std::make_shared<ASTBinaryOp>();
        switch (current_.type) {
            case TokenType::Plus:   op->op = ASTBinaryOp::Op::Add;   break;
            case TokenType::Minus:  op->op = ASTBinaryOp::Op::Sub;   break;
            case TokenType::Star:   op->op = ASTBinaryOp::Op::Mul;   break;
            case TokenType::Slash:  op->op = ASTBinaryOp::Op::Div;   break;
            case TokenType::Percent: op->op = ASTBinaryOp::Op::Div;  break;
            default: break;
        }
        consume();
        auto right = parse_factor();
        op->left = left;
        op->right = right;
        left = op;
    }

    return left;
}

auto Parser::parse_factor() -> std::shared_ptr<ASTExpr> {
    if (current_.type == TokenType::LParen) {
        consume(); // consume (
        auto expr = parse_expression();
        if (current_.type != TokenType::RParen) {
            throw common::Exception{
                "Parser: expected )",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume )
        return expr;
    }

    // Literals
    if (current_.type == TokenType::IntegerLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = static_cast<int64_t>(std::stoll(current_.value));
        consume();
        return lit;
    }

    if (current_.type == TokenType::FloatLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = std::stod(current_.value);
        consume();
        return lit;
    }

    if (current_.type == TokenType::StringLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = current_.value;
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordNull) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = std::monostate{};
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordTrue) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = true;
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordFalse) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = false;
        consume();
        return lit;
    }

    // Identifiers and aggregate function keywords
    if (current_.type == TokenType::Identifier ||
        current_.type == TokenType::KeywordSum ||
        current_.type == TokenType::KeywordCount ||
        current_.type == TokenType::KeywordAvg ||
        current_.type == TokenType::KeywordMin ||
        current_.type == TokenType::KeywordMax) {
        auto col = std::make_shared<ASTColumnRef>();
        col->column = current_.value;
        consume();
        return col;
    }

    throw common::Exception{
        "Parser: unexpected token '" + current_.value + "'",
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto Parser::parse_expression_list() -> std::vector<std::shared_ptr<ASTExpr>> {
    std::vector<std::shared_ptr<ASTExpr>> exprs;
    exprs.push_back(parse_expression());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        exprs.push_back(parse_expression());
    }
    return exprs;
}

auto Parser::parse_table_name() -> std::string {
    if (current_.type != TokenType::Identifier) {
        throw common::Exception{
            "Parser: expected table name",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    std::string name = current_.value;
    consume();
    return name;
}

auto Parser::parse_column_list() -> std::vector<std::string> {
    if (current_.type != TokenType::LParen) {
        throw common::Exception{
            "Parser: expected (",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume (
    std::vector<std::string> cols;
    cols.push_back(current_.value);
    consume();
    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        cols.push_back(current_.value);
        consume();
    }
    if (current_.type != TokenType::RParen) {
        throw common::Exception{
            "Parser: expected )",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume )
    return cols;
}

auto Parser::parse_column_definitions() -> std::vector<ColumnDef> {
    if (current_.type != TokenType::LParen) {
        throw common::Exception{
            "Parser: expected (",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume (
    std::vector<ColumnDef> defs;
    while (current_.type != TokenType::RParen && current_.type != TokenType::EndOfQuery) {
        ColumnDef def;
        def.name = current_.value;
        consume();
        if (current_.type == TokenType::Identifier) {
            def.data_type = current_.value;
            consume();
        }
        defs.push_back(std::move(def));
        if (current_.type == TokenType::Comma) {
            consume(); // consume ,
        }
    }
    if (current_.type == TokenType::RParen) {
        consume(); // consume )
    }
    return defs;
}

auto Parser::parse_value_list() -> std::vector<std::vector<std::string>> {
    std::vector<std::vector<std::string>> values;
    values.push_back(parse_value_row());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        values.push_back(parse_value_row());
    }
    return values;
}

auto Parser::parse_value_row() -> std::vector<std::string> {
    if (current_.type != TokenType::LParen) {
        throw common::Exception{
            "Parser: expected (",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume (
    std::vector<std::string> row;
    row.push_back(current_.value);
    consume();
    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        row.push_back(current_.value);
        consume();
    }
    if (current_.type != TokenType::RParen) {
        throw common::Exception{
            "Parser: expected )",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume )
    return row;
}

auto Parser::parse_order_by_list() -> std::vector<OrderBy> {
    std::vector<OrderBy> orders;
    orders.push_back(parse_order_by());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        orders.push_back(parse_order_by());
    }
    return orders;
}

auto Parser::parse_order_by() -> OrderBy {
    OrderBy order;
    order.column = current_.value;
    consume();
    if (current_.type == TokenType::KeywordAsc || current_.type == TokenType::KeywordDesc) {
        order.direction = current_.type == TokenType::KeywordAsc ?
                  OrderBy::Direction::ASC : OrderBy::Direction::DESC;
        consume();
    }
    return order;
}

auto Parser::parse_limit() -> std::pair<size_t, size_t> {
    size_t count = static_cast<size_t>(std::stoll(current_.value));
    consume();
    size_t offset = 0;
    if (current_.type == TokenType::Comma) {
        consume(); // consume ,
        offset = static_cast<size_t>(std::stoll(current_.value));
        consume();
    }
    return {offset, count};
}

void Parser::consume() {
    current_ = lexer_.next();
}

bool Parser::is_current(TokenType type) const {
    return current_.type == type;
}

// ── QueryParser — concrete SQL query parser ──

auto QueryParser::parse() -> std::unique_ptr<QueryAST> {
    return Parser::parse();  // calls base's parse_query() → std::unique_ptr<QueryAST>
}

} // namespace mnesso::parsers
