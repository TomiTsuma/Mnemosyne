// src/Parsers/parser.h — Parser base class for recursive descent parsing
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <source_location>

namespace mnemo::parsers {

// ── ParseError — result of a parsing failure ──
struct ParseError {
    std::string message;
    Location    location;

    [[nodiscard]] std::string to_string() const {
        return std::string(message + " at " + location.file + ":" +
                   std::to_string(location.line) + ":" + std::to_string(location.column));
    }
};

// ── Parser — base for all recursive descent parsers ──
class Parser {
public:
    Parser(Lexer lexer);

    // Parse entry point — returns AST or error
    virtual auto parse() -> std::unique_ptr<QueryAST> = 0;

    // Helpers
    auto expect(TokenType type) -> std::optional<Token>;
    auto peek()    const -> Token;
    auto advance() -> Token;
    void error(std::string msg);

    // Current position
    [[nodiscard]] Location current_location() const;

protected:
    // ── Query parsing — override in derived classes ──
    auto parse_query() -> std::unique_ptr<QueryAST>;
    auto parse_use(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_select(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_insert(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_create(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_drop(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_alter(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_if_not_exists() -> bool;
    auto parse_if_exists() -> bool;
    auto parse_show(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_describe(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_explain(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_refresh(std::unique_ptr<QueryAST>& ast) -> void;

    // ── Expression parsing helpers ──
    auto parse_expression() -> std::shared_ptr<ASTExpr>;
    auto parse_comparison() -> std::shared_ptr<ASTExpr>;
    auto parse_term() -> std::shared_ptr<ASTExpr>;
    auto parse_factor() -> std::shared_ptr<ASTExpr>;
    auto parse_expression_list() -> std::vector<std::shared_ptr<ASTExpr>>;

    // ── Clause parsing helpers ──
    auto parse_table_name() -> std::string;
    auto parse_table_ref() -> std::pair<std::string, std::string>;
    auto parse_subquery() -> std::shared_ptr<QueryAST>;
    auto parse_window_spec(ASTFunction::WindowSpec& spec) -> void;
    auto parse_database_name() -> std::string;
    auto parse_column_list() -> std::vector<std::string>;
    auto parse_column_definitions() -> std::vector<ColumnDef>;
    auto parse_value_list() -> std::vector<std::vector<std::string>>;
    auto parse_value_row() -> std::vector<std::string>;
    auto parse_order_by_list() -> std::vector<OrderBy>;
    auto parse_order_by() -> OrderBy;
    auto parse_limit() -> std::pair<size_t, size_t>;

    // ── Token helpers ──
    auto consume() -> void;
    [[nodiscard]] auto is_current(TokenType type) const -> bool;

    Lexer lexer_;
    Token  current_;
    bool   has_error_ = false;
};

// ── ParserResult — result of a parsing operation ──
template<typename T>
using ParseResult = std::variant<std::shared_ptr<T>, ParseError>;

} // namespace mnemo::parsers
