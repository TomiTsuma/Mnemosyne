// src/Parsers/ast.h — AST node types for Mnemosyne SQL
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>

namespace mnesso::parsers {

// ── ASTExpr — base for all expression nodes ──
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(const IASTVisitor& visitor) = 0;
    [[nodiscard]] virtual auto to_string() const -> std::string = 0;
};

class IASTVisitor {
public:
    virtual ~IASTVisitor() = default;
    virtual void visit(ASTSelectQuery& n)     = 0;
    virtual void visit(ASTCreateTable& n)     = 0;
    virtual void visit(ASTInsertQuery& n)     = 0;
    virtual void visit(ASTExpr& n)            = 0;
    virtual void visit(ASTLiteral& n)         = 0;
    virtual void visit(ASTColumnRef& n)       = 0;
    virtual void visit(ASTFunction& n)        = 0;
    virtual void visit(ASTBinaryOp& n)        = 0;
    virtual void visit(ASTUnaryOp& n)         = 0;
    virtual void visit(ASTAlias& n)           = 0;
    virtual void visit(ASTSubQuery& n)        = 0;
};

// ── ASTSelectQuery ──
class ASTSelectQuery final : public ASTNode {
public:
    std::vector<std::pair<std::shared_ptr<ASTExpr>, std::optional<std::string>>> select_list;
    std::shared_ptr<ASTFromClause>                from;
    std::shared_ptr<ASTExpr>                      where;
    std::vector<std::shared_ptr<ASTExpr>>         group_by;
    std::shared_ptr<ASTExpr>                      having;
    std::vector<std::pair<std::shared_ptr<ASTExpr>, bool>> order_by;
    std::pair<size_t, std::optional<size_t>>      limit;
    std::optional<std::shared_ptr<ASTSelectQuery>>  left;
    std::optional<std::shared_ptr<ASTSelectQuery>>  right;
    std::string                                   join_type;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTCreateTable ──
class ASTCreateTable final : public ASTNode {
public:
    std::string database;
    std::string table;
    struct ColumnDef {
        std::string name;
        std::string type_name;
        bool        nullable;
        std::optional<std::string> comment;
    };
    std::vector<ColumnDef> columns;
    std::string engine;
    std::string order_by;
    std::string primary_key;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTInsertQuery ──
class ASTInsertQuery final : public ASTNode {
public:
    std::string database;
    std::string table;
    std::vector<std::string> columns;
    std::shared_ptr<ASTSelectQuery> data_query;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTExpr — base for expressions ──
class ASTExpr final : public ASTNode {
public:
    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTLiteral ──
class ASTLiteral final : public ASTNode {
public:
    std::variant<int64_t, double, std::string, bool, std::monostate> value;
    bool is_null() const { return std::holds_alternative<std::monostate>(value); }

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTColumnRef ──
class ASTColumnRef final : public ASTNode {
public:
    std::string table;
    std::string column;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTFunction ──
class ASTFunction final : public ASTNode {
public:
    std::string    name;
    std::vector<std::shared_ptr<ASTExpr>> args;
    bool            is_aggregate;
    std::vector<std::shared_ptr<ASTExpr>> distinct_args;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTBinaryOp ──
class ASTBinaryOp final : public ASTNode {
public:
    enum class Op { Add, Sub, Mul, Div, Eq, Ne, Gt, Lt, Ge, Le, And, Or, Like, NotLike };
    Op       op;
    std::shared_ptr<ASTExpr> left;
    std::shared_ptr<ASTExpr> right;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTUnaryOp ──
class ASTUnaryOp final : public ASTNode {
public:
    enum class Op { Neg, Not, BitwiseNot, Plus };
    Op   op;
    std::shared_ptr<ASTExpr> operand;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTAlias — expression with alias ──
class ASTAlias final : public ASTNode {
public:
    std::shared_ptr<ASTExpr> expression;
    std::string              alias;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTSubQuery — nested query ──
class ASTSubQuery final : public ASTNode {
public:
    std::shared_ptr<ASTSelectQuery> query;
    std::string alias;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

// ── ASTFromClause ──
class ASTFromClause final : public ASTNode {
public:
    struct TableRef {
        std::string table;
        std::string database;
        std::string alias;
        bool        is_global;
    };
    std::vector<TableRef> tables;

    void accept(const IASTVisitor& visitor) override;
    [[nodiscard]] auto to_string() const -> std::string override;
};

} // namespace mnesso::parsers
