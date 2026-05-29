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

namespace mnesso::parsers {

// ── ParseError — result of a parsing failure ──
struct ParseError {
    std::string message;
    Location    location;

    [[nodiscard]] std::string to_string() const {
        return std::format("{} at {}:{}:{}", message, location.file,
                           location.line, location.column);
    }
};

// ── Parser — base for all recursive descent parsers ──
class Parser {
public:
    Parser(Lexer& lexer);

    // Parse entry point — returns AST or error
    virtual auto parse() -> std::variant<std::shared_ptr<ASTNode>, ParseError> = 0;

    // Helpers
    auto expect(TokenType type) -> std::optional<Token>;
    auto peek()    const -> Token;
    auto advance() -> Token;
    void error(std::string msg);

    // Current position
    [[nodiscard]] Location current_location() const;

protected:
    Lexer& lexer_;
    Token  current_;
    bool   has_error_ = false;
};

// ── ParserResult — result of a parsing operation ──
template<typename T>
using ParseResult = std::variant<std::shared_ptr<T>, ParseError>;

} // namespace mnesso::parsers
