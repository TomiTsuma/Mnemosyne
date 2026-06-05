// src/Parsers/ast.h — AST node types for Mnemosyne SQL
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>
#include <cstdint>

namespace mnemo::parsers {

// ── Forward declarations ──
class IASTVisitor;
class ASTUseDatabase;
class ASTSelectQuery;
class ASTCreateTable;
class ASTInsertQuery;
class ASTExpr;
class ASTLiteral;
class ASTColumnRef;
class ASTFunction;
class ASTBinaryOp;
class ASTUnaryOp;
class ASTAlias;
class ASTSubQuery;
class QueryAST;
class ASTFromClause;
class ColumnDef;

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
    virtual void visit(ASTUseDatabase& n)     = 0;
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
    virtual void visit(QueryAST& n)           = 0;
};

// ── ASTUseDatabase ──
class ASTUseDatabase final : public ASTNode {
public:
    std::string database_name;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTUseDatabase"; }
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

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTSelectQuery"; }
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

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTCreateTable"; }
};

// ── ASTInsertQuery ──
class ASTInsertQuery final : public ASTNode {
public:
    std::string database;
    std::string table;
    std::vector<std::string> columns;
    std::shared_ptr<ASTSelectQuery> data_query;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTInsertQuery"; }
};

// ── ASTExpr — base for expressions ──
class ASTExpr : public ASTNode {
public:
    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] virtual std::string to_string() const { return "ASTExpr"; }
};

// ── ASTLiteral ──
class ASTLiteral : public ASTExpr {
public:
    std::variant<int64_t, double, std::string, bool, std::monostate> value;
    bool is_null() const { return std::holds_alternative<std::monostate>(value); }

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTColumnRef ──
class ASTColumnRef : public ASTExpr {
public:
    std::string table;
    std::string column;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTFunction ──
class ASTFunction : public ASTExpr {
public:
    std::string    name;
    std::vector<std::shared_ptr<ASTExpr>> args;
    bool            is_aggregate;
    std::vector<std::shared_ptr<ASTExpr>> distinct_args;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTBinaryOp ──
class ASTBinaryOp : public ASTExpr {
public:
    enum class Op { Add, Sub, Mul, Div, Eq, Ne, Gt, Lt, Ge, Le, And, Or, Like, NotLike };
    Op       op;
    std::shared_ptr<ASTExpr> left;
    std::shared_ptr<ASTExpr> right;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTUnaryOp ──
class ASTUnaryOp : public ASTExpr {
public:
    enum class Op { Neg, Not, BitwiseNot, Plus };
    Op   op;
    std::shared_ptr<ASTExpr> operand;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTAlias — expression with alias ──
class ASTAlias final : public ASTExpr {
public:
    std::shared_ptr<ASTExpr> expression;
    std::string              alias;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTSubQuery — nested query ──
class ASTSubQuery final : public ASTNode {
public:
    std::shared_ptr<ASTSelectQuery> query;
    std::string alias;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
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

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTFromClause"; }
};

// ── QueryAST — top-level parsed query (inherits ASTNode for polymorphism) ──
class QueryAST final : public ASTNode {
public:
    enum class QueryType { SELECT, INSERT, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN, USE };
    QueryType query_type = QueryType::SELECT;

    void accept(const IASTVisitor& visitor) override { }
    [[nodiscard]] auto to_string() const -> std::string override {
        return "QueryAST";
    }

    struct Select {
        std::vector<std::shared_ptr<ASTExpr>> columns;
        std::string table;
        std::shared_ptr<ASTExpr> where;
        std::vector<std::shared_ptr<ASTExpr>> group_by;
        std::shared_ptr<ASTExpr> having;
        std::vector<std::pair<std::string, bool>> order_by;
        std::pair<size_t, size_t> limit = {0, 0};
    } select;

    struct Insert {
        std::string table;
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> values;
    } insert;

    struct Create {
        std::string database_name;
        std::string table_name;
        std::vector<ColumnDef> columns;
    } create;

    struct Drop {
        std::string table_name;
    } drop;

    struct Show {
        enum class ShowType { DATABASES, TABLES };
        ShowType show_type = ShowType::DATABASES;
    } show;

    struct Describe {
        std::string table_name;
    } describe;

    struct Explain {
        std::shared_ptr<QueryAST> explain_query;
    } explain;

    struct Use {
        std::string database_name;
    } use;
};

// ── Expression — base for expression AST nodes ──
class Expression {
public:
    enum class OpType {
        AND, OR, NOT,
        Plus, Minus, Star, Slash, Percent,
        Eq, Ne, Lt, Gt, Le, Ge,
        Concat
    };

    OpType op = OpType::AND;
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    explicit Expression(OpType op = OpType::AND,
                        std::shared_ptr<Expression> l = nullptr,
                        std::shared_ptr<Expression> r = nullptr)
        : op(op), left(std::move(l)), right(std::move(r)) {}
    virtual ~Expression() = default;

    [[nodiscard]]
    virtual std::string to_string() const {
        return "Expression";
    }
};

// ── Literal — a literal value node ──
class Literal : public Expression {
public:
    std::string value;
    explicit Literal(const std::string& val) : Expression(), value(val) {}

    [[nodiscard]]
    std::string to_string() const override;
};

// ── Identifier — a column/table name node ──
class Identifier : public Expression {
public:
    std::string name;
    explicit Identifier(const std::string& n) : Expression(), name(n) {}

    [[nodiscard]]
    std::string to_string() const override;
};

// ── ColumnDef ──
struct ColumnDef {
    std::string name;
    std::string data_type;
};

// ── OrderBy ──
struct OrderBy {
    std::string column;
    enum class Direction { ASC, DESC };
    Direction direction = Direction::ASC;
};

} // namespace mnemo::parsers
