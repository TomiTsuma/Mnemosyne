// src/Parsers/lexer.h — SQL lexer (tokenizer) for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstdint>

namespace mnemo::parsers {

// ── Token type enumeration ──
enum class TokenType {
    // Keywords
    KeywordSelect, KeywordInsert, KeywordCreate, KeywordDrop,
    KeywordShow, KeywordDescribe, KeywordDesc, KeywordExplain,
    KeywordFrom, KeywordWhere, KeywordGroup, KeywordBy,
    KeywordHaving, KeywordOrder, KeywordLimit, KeywordOffset,
    KeywordInto, KeywordTable, KeywordJoin, KeywordLeft,
    KeywordRight, KeywordInner, KeywordOuter, KeywordOn,
    KeywordAnd, KeywordOr, KeywordNot, KeywordAs,
    KeywordIs, KeywordNull, KeywordTrue, KeywordFalse,
    KeywordWith, KeywordRollup, KeywordArray, KeywordLateral,
    KeywordAny, KeywordAll, KeywordDistinct, KeywordValues,
    KeywordAsc, KeywordSum, KeywordCount,
    KeywordAvg, KeywordMin, KeywordMax,
    KeywordDatabase, KeywordUse,

    // Literals
    IntegerLiteral, FloatLiteral, StringLiteral,

    // Operators
    Plus, Minus, Star, Slash, Percent,
    Eq, Ne, Lt, Gt, Le, Ge,
    Concat,

    // Delimiters
    Comma, Dot, LParen, RParen, LSquare, RSquare,

    // Identifiers
    Identifier,

    // Punctuation
    Semicolon, Backtick,

    // Special
    EndOfQuery
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

    Token() : type(TokenType::EndOfQuery), value(""), location() {}
    Token(TokenType t, std::string v, Location loc)
        : type(t), value(std::move(v)), location(loc) {}

    [[nodiscard]] auto is(TokenType t) const -> bool { return type == t; }
    [[nodiscard]] auto to_string() const -> std::string {
        return std::string("Token(") + std::to_string(static_cast<int>(type)) +
               ", '" + value + "', " + location.file + ":" +
               std::to_string(location.line) + ":" + std::to_string(location.column) + ")";
    }
};

// ── Lexer — converts SQL text into tokens ──
class Lexer {
public:
    explicit Lexer(std::string source, std::string file = "");

    // Advance to next token — returns token or EOF
    auto next() -> Token;

    // Peek at current token without advancing
    [[nodiscard]] auto peek() -> Token;

    // Reset to beginning
    void reset();

    // Check for EOF
    [[nodiscard]] auto eof() const -> bool;

private:
    void skip_whitespace();
    void skip_line_comment();
    void skip_block_comment();
    Token read_string();
    Token read_number();
    Token read_identifier();
    Token make_token(TokenType type, std::string value) const;
    void advance_pos(size_t n);

    std::string       source_;
    size_t            pos_  = 0;
    uint32_t          line_ = 1;
    uint32_t          col_  = 1;
    std::string       file_;
};

} // namespace mnemo::parsers
