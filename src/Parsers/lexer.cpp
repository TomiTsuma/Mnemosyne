// src/Parsers/lexer.cpp — SQL lexer for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Parsers/lexer.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::parsers {

// ── Lexer ──

Lexer::Lexer(std::string source, std::string file)
    : source_{std::move(source)}, file_{std::move(file)} {}

auto Lexer::next() -> Token {
    skip_whitespace();
    if (pos_ >= source_.size()) {
        return Token{TokenType::EndOfQuery, "", {file_, line_, col_}};
    }

    char c = source_[pos_];

    // Single-character tokens
    switch (c) {
        case '(': {
            auto token = make_token(TokenType::LParen, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ')': {
            auto token = make_token(TokenType::RParen, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ',': {
            auto token = make_token(TokenType::Comma, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '.': {
            if (pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
                return read_number();
            }
            auto token = make_token(TokenType::Dot, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ';': {
            auto token = make_token(TokenType::Semicolon, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '*': {
            auto token = make_token(TokenType::Star, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '+': {
            auto token = make_token(TokenType::Plus, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '%': {
            auto token = make_token(TokenType::Percent, std::string(1, c));
            advance_pos(1);
            return token;
        }
    }

    // - or ->
    if (c == '-') {
        size_t next = pos_ + 1;
        if (next < source_.size() && source_[next] == '>') {
            advance_pos(2);
            return make_token(TokenType::Concat, "->");
        }
        auto token = make_token(TokenType::Minus, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // /
    if (c == '/') {
        // Check for // comment
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            skip_line_comment();
            return next();
        }
        // Check for /* comment */
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
            skip_block_comment();
            return next();
        }
        auto token = make_token(TokenType::Slash, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // = or != or <> or <= or >=
    if (c == '=') {
        auto token = make_token(TokenType::Eq, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '!') {
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
            advance_pos(2);
            return make_token(TokenType::Ne, "!=");
        }
        auto token = make_token(TokenType::Ne, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '<') {
        if (pos_ + 1 < source_.size()) {
            if (source_[pos_ + 1] == '=') {
                advance_pos(2);
                return make_token(TokenType::Le, "<=");
            }
            if (source_[pos_ + 1] == '>') {
                advance_pos(2);
                return make_token(TokenType::Ne, "<>");
            }
        }
        auto token = make_token(TokenType::Lt, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '>') {
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
            advance_pos(2);
            return make_token(TokenType::Ge, ">=");
        }
        auto token = make_token(TokenType::Gt, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // Numbers (integers and floats)
    if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1]))) {
        return read_number();
    }

    // Strings
    if (c == '\'') {
        return read_string();
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return read_identifier();
    }

    // Unknown character
    advance_pos(1);
    return make_token(TokenType::Identifier, std::string(1, c));
}

void Lexer::skip_whitespace() {
    while (pos_ < source_.size() && std::isspace(source_[pos_])) {
        pos_++;
        if (source_[pos_ - 1] == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
    }
}

void Lexer::skip_line_comment() {
    pos_ += 2; // skip //
    while (pos_ < source_.size() && source_[pos_] != '\n') {
        pos_++;
        col_++;
    }
}

void Lexer::skip_block_comment() {
    pos_ += 2; // skip /*
    while (pos_ + 1 < source_.size()) {
        if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
            pos_ += 2;
            col_ += 2;
            return;
        }
        pos_++;
        col_++;
    }
}

auto Lexer::read_number() -> Token {
    size_t start = pos_;
    bool has_dot = false;

    while (pos_ < source_.size()) {
        if (source_[pos_] == '.') {
            if (has_dot) break;
            has_dot = true;
        } else if (!std::isdigit(source_[pos_])) {
            break;
        }
        pos_++;
        col_++;
    }

    std::string num_str{source_.substr(start, pos_ - start)};
    return make_token(has_dot ? TokenType::FloatLiteral : TokenType::IntegerLiteral, num_str);
}

auto Lexer::read_string() -> Token {
    size_t start = pos_;
    pos_++; // skip opening quote
    std::string str;
    col_++;

    while (pos_ < source_.size() && source_[pos_] != '\'') {
        str += source_[pos_];
        pos_++;
        col_++;
    }

    if (pos_ < source_.size()) {
        pos_++; // skip closing quote
        col_++;
    }

    return make_token(TokenType::StringLiteral, std::move(str));
}

auto Lexer::read_identifier() -> Token {
    size_t start = pos_;
    while (pos_ < source_.size() && (std::isalnum(source_[pos_]) || source_[pos_] == '_')) {
        pos_++;
        col_++;
    }

    std::string id{source_.substr(start, pos_ - start)};
    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    TokenType type = TokenType::Identifier;
    if (upper == "SELECT") type = TokenType::KeywordSelect;
    else if (upper == "FROM") type = TokenType::KeywordFrom;
    else if (upper == "WHERE") type = TokenType::KeywordWhere;
    else if (upper == "ORDER") type = TokenType::KeywordOrder;
    else if (upper == "BY") type = TokenType::KeywordBy;
    else if (upper == "GROUP") type = TokenType::KeywordGroup;
    else if (upper == "HAVING") type = TokenType::KeywordHaving;
    else if (upper == "LIMIT") type = TokenType::KeywordLimit;
    else if (upper == "OFFSET") type = TokenType::KeywordOffset;
    else if (upper == "AS") type = TokenType::KeywordAs;
    else if (upper == "AND") type = TokenType::KeywordAnd;
    else if (upper == "OR") type = TokenType::KeywordOr;
    else if (upper == "NOT") type = TokenType::KeywordNot;
    else if (upper == "NULL") type = TokenType::KeywordNull;
    else if (upper == "TRUE") type = TokenType::KeywordTrue;
    else if (upper == "FALSE") type = TokenType::KeywordFalse;
    else if (upper == "SUM") type = TokenType::KeywordSum;
    else if (upper == "COUNT") type = TokenType::KeywordCount;
    else if (upper == "AVG") type = TokenType::KeywordAvg;
    else if (upper == "MIN") type = TokenType::KeywordMin;
    else if (upper == "MAX") type = TokenType::KeywordMax;
    else if (upper == "INSERT") type = TokenType::KeywordInsert;
    else if (upper == "INTO") type = TokenType::KeywordInto;
    else if (upper == "VALUES") type = TokenType::KeywordValues;
    else if (upper == "CREATE") type = TokenType::KeywordCreate;
    else if (upper == "TABLE") type = TokenType::KeywordTable;
    else if (upper == "DROP") type = TokenType::KeywordDrop;
    else if (upper == "SHOW") type = TokenType::KeywordShow;
    else if (upper == "DATABASES") type = TokenType::KeywordShow;
    else if (upper == "DATABASE") type = TokenType::KeywordDatabase;
    else if (upper == "TABLES") type = TokenType::KeywordTable;
    else if (upper == "DESCRIBE") type = TokenType::KeywordDescribe;
    else if (upper == "DESC") type = TokenType::KeywordDesc;
    else if (upper == "EXPLAIN") type = TokenType::KeywordExplain;
    else if (upper == "JOIN") type = TokenType::KeywordJoin;
    else if (upper == "LEFT") type = TokenType::KeywordLeft;
    else if (upper == "RIGHT") type = TokenType::KeywordRight;
    else if (upper == "INNER") type = TokenType::KeywordInner;
    else if (upper == "ON") type = TokenType::KeywordOn;
    else if (upper == "UNION") type = TokenType::KeywordAll;
    else if (upper == "ALL") type = TokenType::KeywordAll;
    else if (upper == "WITH") type = TokenType::KeywordWith;
    else if (upper == "ROLLUP") type = TokenType::KeywordRollup;
    else if (upper == "ARRAY") type = TokenType::KeywordArray;
    else if (upper == "LATERAL") type = TokenType::KeywordLateral;
    else if (upper == "ANY") type = TokenType::KeywordAny;
    else if (upper == "DISTINCT") type = TokenType::KeywordDistinct;
    else if (upper == "ASC") type = TokenType::KeywordAsc;
    else if (upper == "DESC") type = TokenType::KeywordDesc;
    else if (upper == "USE") type = TokenType::KeywordUse;
    else if (upper == "ALTER") type = TokenType::KeywordAlter;
    else if (upper == "IF") type = TokenType::KeywordIf;
    else if (upper == "EXISTS") type = TokenType::KeywordExists;
    else if (upper == "ENGINE") type = TokenType::KeywordEngine;
    else if (upper == "TRUNCATE") type = TokenType::KeywordTruncate;
    else if (upper == "DETACH") type = TokenType::KeywordDetach;
    else if (upper == "ADD") type = TokenType::KeywordAdd;
    else if (upper == "COLUMN") type = TokenType::KeywordColumn;
    else if (upper == "MODIFY") type = TokenType::KeywordModify;
    else if (upper == "IN") type = TokenType::KeywordIn;
    else if (upper == "OVER") type = TokenType::KeywordOver;
    else if (upper == "PARTITION") type = TokenType::KeywordPartition;
    else if (upper == "VIEW") type = TokenType::KeywordView;
    else if (upper == "VIEWS") type = TokenType::KeywordViews;
    else if (upper == "MATERIALIZED") type = TokenType::KeywordMaterialized;
    else if (upper == "REFRESH") type = TokenType::KeywordRefresh;
    else if (upper == "STORAGE") type = TokenType::KeywordStorage;
    else if (upper == "UNIT") type = TokenType::KeywordUnit;
    else if (upper == "UNITS") type = TokenType::KeywordUnits;
    else if (upper == "TYPE") type = TokenType::KeywordType;
    else if (upper == "PATH") type = TokenType::KeywordPath;
    else if (upper == "BUCKET") type = TokenType::KeywordBucket;
    else if (upper == "ENDPOINT") type = TokenType::KeywordEndpoint;
    else if (upper == "REGION") type = TokenType::KeywordRegion;
    else if (upper == "USAGE") type = TokenType::KeywordUsage;

    return make_token(type, std::move(id));
}

auto Lexer::make_token(TokenType type, std::string value) const -> Token {
    std::fprintf(stderr, "Lexer::make_token type=%d value='%s' line=%u col=%u\n",
                 static_cast<int>(type), value.c_str(), line_, col_);
    return Token{type, std::move(value), {file_, line_, col_}};
}

void Lexer::advance_pos(size_t n) {
    pos_ += n;
    col_ += n;
}

auto Lexer::peek() -> Token {
    size_t saved_pos = pos_;
    uint32_t saved_line = line_;
    uint32_t saved_col = col_;
    Token t = next();
    pos_  = saved_pos;
    line_ = saved_line;
    col_  = saved_col;
    return t;
}

void Lexer::reset() {
    pos_ = 0;
    line_ = 1;
    col_ = 1;
}

auto Lexer::eof() const -> bool {
    return pos_ >= source_.size();
}

} // namespace mnemo::parsers
