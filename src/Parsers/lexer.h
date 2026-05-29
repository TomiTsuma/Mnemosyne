// src/Parsers/lexer.h — SQL lexer (tokenizer) for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstdint>

namespace mnesso::parsers {

// ── Token type enumeration ──
enum class TokenType {
    // Keywords
    SELECT, FROM, WHERE, GROUP, BY, HAVING, ORDER, LIMIT, OFFSET,
    INSERT, INTO, CREATE, TABLE, DROP, ALTER, UPDATE, DELETE,
    JOIN, LEFT, RIGHT, INNER, OUTER, ON, CROSS, FULL,
    AND, OR, NOT, AS, IS, NULL, TRUE, FALSE,
    WITH, ROLLUP, ARRAY, LATERAL, ANY, ALL, DISTINCT,

    // Literals
    INTEGER_LITERAL, FLOAT_LITERAL, STRING_LITERAL,

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NE, LT, GT, LE, GE,
    CONCAT,

    // Delimiters
    COMMA, DOT, LPAREN, RPAREN, LSQUARE, RSQUARE,

    // Identifiers
    IDENTIFIER,

    // Punctuation
    SEMICOLON, BACKTICK,
};

// ── Location in source ──
struct Location {
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
};

// ── Token — a single lexical unit ──
struct Token {
    TokenType type;
    std::string value;
    Location    location;

    [[nodiscard]] auto is(TokenType t) const -> bool { return type == t; }
    [[nodiscard]] auto to_string() const -> std::string {
        return std::format("Token({}, '{}', {}:{}:{})",
                           static_cast<int>(type), value,
                           location.file, location.line, location.column);
    }
};

// ── Lexer — converts SQL text into tokens ──
class Lexer {
public:
    explicit Lexer(std::string_view source, std::string file = "");

    // Advance to next token — returns token or EOF
    auto next() -> Token;

    // Peek at current token without advancing
    [[nodiscard]] auto peek() const -> Token;

    // Reset to beginning
    void reset();

    // Check for EOF
    [[nodiscard]] auto eof() const -> bool;

private:
    auto skip_whitespace();
    auto read_string();
    auto read_number();
    auto read_identifier();
    auto read_operator();
    auto make_token(TokenType type, std::string_view value) const -> Token;

    std::string_view  source_;
    size_t            pos_  = 0;
    uint32_t          line_ = 1;
    uint32_t          col_  = 1;
    std::string       file_;
};

} // namespace mnesso::parsers
