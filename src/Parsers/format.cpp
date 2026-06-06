// src/Parsers/format.cpp — SQL formatting utilities
// Mnemosyne: A column-oriented analytical DBMS

#include "ast.h"
#include <algorithm>
#include <sstream>
#include <variant>
#include <string>

namespace mnemo::parsers {

// ── format_query — human-readable SQL from QueryAST ──

std::string format_query(const QueryAST& query);
std::string format_query(const ASTSelectQuery& query);

// ── Helper: convert ASTExpr to string (dispatches on dynamic type) ──

static std::string expression_to_string(const ASTExpr& expr);

static std::string expression_to_string(const ASTExpr& expr) {
    if (auto* lit = dynamic_cast<const ASTLiteral*>(&expr))
        return lit->to_string();
    if (auto* col = dynamic_cast<const ASTColumnRef*>(&expr))
        return col->to_string();
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr))
        return func->to_string();
    if (auto* binop = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        std::string sep;
        switch (binop->op) {
            case ASTBinaryOp::Op::Add:     sep = " + "; break;
            case ASTBinaryOp::Op::Sub:     sep = " - "; break;
            case ASTBinaryOp::Op::Mul:     sep = " * "; break;
            case ASTBinaryOp::Op::Div:     sep = " / "; break;
            case ASTBinaryOp::Op::Eq:      sep = " = "; break;
            case ASTBinaryOp::Op::Ne:      sep = " != "; break;
            case ASTBinaryOp::Op::Lt:      sep = " < "; break;
            case ASTBinaryOp::Op::Gt:      sep = " > "; break;
            case ASTBinaryOp::Op::Le:      sep = " <= "; break;
            case ASTBinaryOp::Op::Ge:      sep = " >= "; break;
            case ASTBinaryOp::Op::And:     sep = " AND "; break;
            case ASTBinaryOp::Op::Or:      sep = " OR ";  break;
            case ASTBinaryOp::Op::Like:    sep = " LIKE "; break;
            case ASTBinaryOp::Op::NotLike: sep = " NOT LIKE "; break;
            default: sep = " ";
        }
        return "(" + expression_to_string(*binop->left) + sep + expression_to_string(*binop->right) + ")";
    }
    if (auto* unop = dynamic_cast<const ASTUnaryOp*>(&expr)) {
        std::string prefix;
        switch (unop->op) {
            case ASTUnaryOp::Op::Neg:    prefix = "-"; break;
            case ASTUnaryOp::Op::Not:    prefix = "NOT "; break;
            case ASTUnaryOp::Op::BitwiseNot: prefix = "~"; break;
            case ASTUnaryOp::Op::Plus:   prefix = "+"; break;
            default: prefix = "";
        }
        return prefix + "(" + expression_to_string(*unop->operand) + ")";
    }
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        return expression_to_string(*alias->expression) + " AS " + alias->alias;
    }
    if (auto* subq = dynamic_cast<const ASTSubQuery*>(&expr)) {
        return "(" + format_query(*subq->query) + ")";
    }
    return "??";
}

// ── ASTExpr subclass to_string() implementations ──

std::string ASTLiteral::to_string() const {
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "NULL";
        if constexpr (std::is_same_v<T, int64_t>) return std::to_string(v);
        if constexpr (std::is_same_v<T, double>) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }
        if constexpr (std::is_same_v<T, bool>) return v ? "TRUE" : "FALSE";
        if constexpr (std::is_same_v<T, std::string>) return "'" + v + "'";
    }, value);
}

std::string ASTColumnRef::to_string() const {
    if (!table.empty()) return table + "." + column;
    return column;
}

std::string ASTFunction::to_string() const {
    std::ostringstream out;
    out << name << "(";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) out << ", ";
        out << expression_to_string(*args[i]);
    }
    out << ")";
    return out.str();
}

std::string ASTBinaryOp::to_string() const {
    std::string sep;
    switch (op) {
        case Op::Add:      sep = " + "; break;
        case Op::Sub:      sep = " - "; break;
        case Op::Mul:      sep = " * "; break;
        case Op::Div:      sep = " / "; break;
        case Op::Eq:       sep = " = "; break;
        case Op::Ne:       sep = " != "; break;
        case Op::Lt:       sep = " < "; break;
        case Op::Gt:       sep = " > "; break;
        case Op::Le:       sep = " <= "; break;
        case Op::Ge:       sep = " >= "; break;
        case Op::And:      sep = " AND "; break;
        case Op::Or:       sep = " OR ";  break;
        case Op::Like:     sep = " LIKE "; break;
        case Op::NotLike:  sep = " NOT LIKE "; break;
        default:           sep = " ";
    }
    return "(" + expression_to_string(*left) + sep + expression_to_string(*right) + ")";
}

std::string ASTUnaryOp::to_string() const {
    std::string prefix;
    switch (op) {
        case Op::Neg:         prefix = "-"; break;
        case Op::Not:         prefix = "NOT "; break;
        case Op::BitwiseNot:  prefix = "~"; break;
        case Op::Plus:        prefix = "+"; break;
        default:              prefix = "";
    }
    return prefix + "(" + expression_to_string(*operand) + ")";
}

std::string ASTAlias::to_string() const {
    return expression_to_string(*expression) + " AS " + alias;
}

std::string ASTSubQuery::to_string() const {
    return "(" + format_query(*query) + ")";
}

// ── format_query — human-readable SQL from QueryAST ──

std::string format_query(const QueryAST& query) {
    std::ostringstream out;
    switch (query.query_type) {
        case QueryAST::QueryType::SELECT: {
            const auto& s = query.select;
            out << "SELECT ";
            for (size_t i = 0; i < s.columns.size(); ++i) {
                if (i > 0) out << ", ";
                out << expression_to_string(*s.columns[i]);
            }
            out << "\nFROM " << s.table;
            if (s.where) {
                out << "\nWHERE " << expression_to_string(*s.where);
            }
            if (!s.group_by.empty()) {
                out << "\nGROUP BY ";
                for (size_t i = 0; i < s.group_by.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << expression_to_string(*s.group_by[i]);
                }
            }
            if (s.having) {
                out << "\nHAVING " << expression_to_string(*s.having);
            }
            if (!s.order_by.empty()) {
                out << "\nORDER BY ";
                for (size_t i = 0; i < s.order_by.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << s.order_by[i].first;
                    out << (s.order_by[i].second ? " DESC" : " ASC");
                }
            }
            if (s.limit.first > 0) {
                out << "\nLIMIT " << s.limit.second << " OFFSET " << s.limit.first;
            } else if (s.limit.second > 0) {
                out << "\nLIMIT " << s.limit.second;
            }
            break;
        }
        case QueryAST::QueryType::INSERT: {
            const auto& ins = query.insert;
            out << "INSERT INTO " << ins.table;
            if (!ins.columns.empty()) {
                out << " (";
                for (size_t i = 0; i < ins.columns.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << ins.columns[i];
                }
                out << ")";
            }
            if (!ins.values.empty()) {
                out << "\nVALUES ";
                for (size_t i = 0; i < ins.values.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << "(";
                    for (size_t j = 0; j < ins.values[i].size(); ++j) {
                        if (j > 0) out << ", ";
                        out << ins.values[i][j];
                    }
                    out << ")";
                }
            }
            break;
        }
        case QueryAST::QueryType::CREATE: {
            const auto& c = query.create;
            switch (c.kind) {
                case QueryAST::Create::Kind::Database:
                    out << "CREATE DATABASE " << c.database_name;
                    break;
                case QueryAST::Create::Kind::View:
                    out << "CREATE VIEW " << c.view_name << " AS SELECT ...";
                    break;
                case QueryAST::Create::Kind::MaterializedView:
                    out << "CREATE MATERIALIZED VIEW " << c.view_name << " AS SELECT ...";
                    break;
                case QueryAST::Create::Kind::Table:
                default:
                    out << "CREATE TABLE " << c.table_name << " (\n";
                    for (size_t i = 0; i < c.columns.size(); ++i) {
                        out << "  " << c.columns[i].name << " " << c.columns[i].data_type;
                        if (i + 1 < c.columns.size()) out << ",";
                        out << "\n";
                    }
                    out << ")";
                    break;
            }
            break;
        }
        case QueryAST::QueryType::DROP: {
            switch (query.drop.object_kind) {
                case QueryAST::ObjectKind::View:
                    out << "DROP VIEW " << query.drop.table;
                    break;
                case QueryAST::ObjectKind::MaterializedView:
                    out << "DROP MATERIALIZED VIEW " << query.drop.table;
                    break;
                case QueryAST::ObjectKind::Table:
                default:
                    out << "DROP TABLE " << query.drop.table;
                    break;
            }
            break;
        }
        case QueryAST::QueryType::REFRESH: {
            out << "REFRESH MATERIALIZED VIEW " << query.refresh.name;
            break;
        }
        case QueryAST::QueryType::ALTER: {
            out << "ALTER TABLE " << query.alter.table;
            for (const auto& cmd : query.alter.commands) {
                out << " ";
                switch (cmd.type) {
                    case ASTAlterQuery::AlterCommand::Type::ADD_COLUMN:
                        out << "ADD COLUMN " << cmd.column_name << " " << cmd.column_type;
                        break;
                    case ASTAlterQuery::AlterCommand::Type::DROP_COLUMN:
                        out << "DROP COLUMN " << cmd.column_name;
                        break;
                    case ASTAlterQuery::AlterCommand::Type::MODIFY_COLUMN:
                        out << "MODIFY COLUMN " << cmd.column_name << " " << cmd.column_type;
                        break;
                }
            }
            break;
        }
        case QueryAST::QueryType::SHOW: {
            switch (query.show.show_type) {
                case QueryAST::Show::ShowType::DATABASES: out << "SHOW DATABASES"; break;
                case QueryAST::Show::ShowType::TABLES: out << "SHOW TABLES"; break;
                case QueryAST::Show::ShowType::VIEWS: out << "SHOW VIEWS"; break;
                case QueryAST::Show::ShowType::MATERIALIZED_VIEWS:
                    out << "SHOW MATERIALIZED_VIEWS"; break;
            }
            break;
        }
        case QueryAST::QueryType::DESCRIBE: {
            switch (query.describe.object_kind) {
                case QueryAST::ObjectKind::View:
                    out << "DESCRIBE VIEW " << query.describe.table_name;
                    break;
                case QueryAST::ObjectKind::MaterializedView:
                    out << "DESCRIBE MATERIALIZED VIEW " << query.describe.table_name;
                    break;
                case QueryAST::ObjectKind::Table:
                default:
                    out << "DESCRIBE " << query.describe.table_name;
                    break;
            }
            break;
        }
        case QueryAST::QueryType::EXPLAIN: {
            out << "EXPLAIN ";
            out << format_query(*query.explain.explain_query);
            break;
        }
    }
    return out.str();
}

// ── format_query overload for ASTSelectQuery ──

std::string format_query(const ASTSelectQuery& query) {
    std::ostringstream out;
    out << "SELECT ";
    for (size_t i = 0; i < query.select_list.size(); ++i) {
        if (i > 0) out << ", ";
        out << expression_to_string(*query.select_list[i].first);
        if (query.select_list[i].second)
            out << " AS " << *query.select_list[i].second;
    }
    if (query.from && !query.from->tables.empty())
        out << "\nFROM " << query.from->tables[0].table;
    else
        out << "\nFROM ";
    if (query.where)
        out << "\nWHERE " << expression_to_string(*query.where);
    if (!query.group_by.empty()) {
        out << "\nGROUP BY ";
        for (size_t i = 0; i < query.group_by.size(); ++i) {
            if (i > 0) out << ", ";
            out << expression_to_string(*query.group_by[i]);
        }
    }
    if (query.having)
        out << "\nHAVING " << expression_to_string(*query.having);
    if (!query.order_by.empty()) {
        out << "\nORDER BY ";
        for (size_t i = 0; i < query.order_by.size(); ++i) {
            if (i > 0) out << ", ";
            out << expression_to_string(*query.order_by[i].first);
            out << (query.order_by[i].second ? " DESC" : " ASC");
        }
    }
    if (query.limit.first > 0)
        out << "\nOFFSET " << query.limit.first;
    if (query.limit.second.has_value() && query.limit.second.value() > 0)
        out << "\nLIMIT " << query.limit.second.value();
    return out.str();
}

} // namespace mnemo::parsers
