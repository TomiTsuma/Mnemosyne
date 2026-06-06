// src/Databases/view_catalog.cpp — View catalog helpers

#include "view_catalog.h"
#include <unordered_set>

namespace mnemo::databases {

namespace {

void collect_from_expr(const std::shared_ptr<parsers::ASTExpr>& expr,
                       std::unordered_set<std::string>& out) {
    if (!expr) {
        return;
    }
    if (auto* col = dynamic_cast<parsers::ASTColumnRef*>(expr.get())) {
        if (!col->table.empty()) {
            out.insert(col->table);
        }
    } else if (auto* bin = dynamic_cast<parsers::ASTBinaryOp*>(expr.get())) {
        collect_from_expr(bin->left, out);
        collect_from_expr(bin->right, out);
    } else if (auto* un = dynamic_cast<parsers::ASTUnaryOp*>(expr.get())) {
        collect_from_expr(un->operand, out);
    } else if (auto* fn = dynamic_cast<parsers::ASTFunction*>(expr.get())) {
        for (const auto& arg : fn->args) {
            collect_from_expr(arg, out);
        }
    } else if (auto* alias = dynamic_cast<parsers::ASTAlias*>(expr.get())) {
        collect_from_expr(alias->expression, out);
    } else if (auto* sq = dynamic_cast<parsers::ASTSubQueryExpr*>(expr.get())) {
        if (sq->query && sq->query->query_type == parsers::QueryAST::QueryType::SELECT) {
            for (const auto& dep : extract_dependencies(sq->query->select)) {
                out.insert(dep);
            }
        }
    }
}

} // namespace

auto extract_dependencies(const parsers::QueryAST::Select& select)
    -> std::vector<std::string> {
    std::unordered_set<std::string> deps;

    if (!select.table.empty()) {
        deps.insert(select.table);
    }
    for (const auto& join : select.joins) {
        if (!join.table.empty()) {
            deps.insert(join.table);
        }
    }
    for (const auto& col_expr : select.columns) {
        collect_from_expr(col_expr, deps);
    }
    if (select.where) {
        collect_from_expr(select.where, deps);
    }
    for (const auto& gb : select.group_by) {
        collect_from_expr(gb, deps);
    }
    if (select.having) {
        collect_from_expr(select.having, deps);
    }

    return {deps.begin(), deps.end()};
}

} // namespace mnemo::databases
