// src/Analyzer/analyzer.cpp — Query analyzer: semantic analysis of parsed AST
// Mnemosyne: A column-oriented analytical DBMS

#include "Analyzer/analyzer.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <unordered_set>

namespace mnesso::analyzer {

using interpreters::Context;

// The header declares Analyzer(Context&) — it works against the
// interpreters::Context which is the single source of truth for
// databases, storages, settings, and query info.
// The .cpp must match this signature exactly.
Analyzer::Analyzer(Context& context) : context_{context} {}

auto Analyzer::analyze(std::shared_ptr<parsers::QueryAST> ast) -> AnalyzeResult {
    AnalyzeResult result;
    result.valid = true;

    // QueryAST is a container — extract the appropriate node type
    // based on query_type and analyze it with the visitor.
    switch (ast->query_type) {
        case parsers::QueryAST::QueryType::SELECT: {
            // Build a temporary ASTSelectQuery from QueryAST::select
            auto select_query = std::make_shared<parsers::ASTSelectQuery>();
            // TODO: map QueryAST::select fields to ASTSelectQuery
            result.analyzed_ast = select_query;
            break;
        }
        case parsers::QueryAST::QueryType::INSERT: {
            auto insert_query = std::make_shared<parsers::ASTInsertQuery>();
            result.analyzed_ast = insert_query;
            break;
        }
        case parsers::QueryAST::QueryType::CREATE: {
            auto create_query = std::make_shared<parsers::ASTCreateTable>();
            result.analyzed_ast = create_query;
            break;
        }
        default:
            result.analyzed_ast = std::make_shared<parsers::ASTExpr>();
            break;
    }

    return result;
}

auto Analyzer::resolve_tables(AnalyzeResult& result,
                              std::shared_ptr<parsers::ASTFromClause> from)
    -> std::vector<std::string> {
    std::vector<std::string> errors;

    if (!from) {
        // FROM is optional (e.g. SELECT 1)
        return errors;
    }

    for (const auto& ref : from->tables) {
        auto storage = context_.get_storage(ref.table);
        if (!storage) {
            errors.push_back("Unknown table: " + ref.table);
            result.unresolved_columns.insert(ref.table);
            continue;
        }

        // Register the table in the result
        result.tables[ref.alias.empty() ? ref.table : ref.alias] =
            Context::TableInfo{ref.table, ref.database};
    }

    return errors;
}

auto Analyzer::resolve_columns(AnalyzeResult& result,
                               std::shared_ptr<parsers::ASTExpr> expr)
    -> std::vector<std::string> {
    std::vector<std::string> errors;

    if (!expr) return errors;

    // Collect columns from resolved tables
    for (const auto& [alias, table_info] : result.tables) {
        auto storage = context_.get_storage(table_info.name);
        if (!storage) continue;

        // TODO: iterate storage columns and match against expr
        // For now, record the alias as a valid table reference
        result.columns[alias] = Context::ColumnInfo{table_info.name, "unknown"};
    }

    return errors;
}

auto Analyzer::validate_function(AnalyzeResult& result,
                                 std::shared_ptr<parsers::ASTFunction> func)
    -> std::vector<std::string> {
    std::vector<std::string> errors;

    // TODO: validate function name and arguments against known functions
    (void)result;
    (void)func;

    return errors;
}

auto Analyzer::has_errors(const AnalyzeResult& result) const -> bool {
    return !result.errors.empty();
}

// ── buildQueryTree — convert analyzed AST + metadata into IR ──

auto Analyzer::buildQueryTree(AnalyzeResult& result)
    -> std::shared_ptr<IQueryTreeNode> {
    if (!result.valid || !result.analyzed_ast) {
        return nullptr;
    }

    auto& ast = result.analyzed_ast;

    // The analyzed_ast is a QueryAST (set in analyze()) — dispatch by query type
    // We cast via the concrete type set in analyze()
    if (auto* query_ast = dynamic_cast<parsers::QueryAST*>(ast.get())) {
        switch (query_ast->query_type) {
            case parsers::QueryAST::QueryType::SELECT:
                return buildSelectNode(result);
            case parsers::QueryAST::QueryType::INSERT:
            case parsers::QueryAST::QueryType::CREATE:
            case parsers::QueryAST::QueryType::DROP:
            case parsers::QueryAST::QueryType::SHOW:
            case parsers::QueryAST::QueryType::DESCRIBE:
            case parsers::QueryAST::QueryType::EXPLAIN:
                // These produce simpler trees — TableNode for now
                return buildTableNode("unknown", "unknown");
        }
    }

    return nullptr;
}

auto Analyzer::buildTableNode(const std::string& table_name,
                              const std::string& db_name)
    -> std::shared_ptr<TableNode> {
    auto node = std::make_shared<TableNode>();
    node->table   = table_name;
    node->database = db_name;
    return node;
}

auto Analyzer::buildSelectNode(const AnalyzeResult& result)
    -> std::shared_ptr<SelectNode> {
    auto node = std::make_shared<SelectNode>();

    // Build column expressions from the analyzed AST's select list
    if (auto* query_ast = dynamic_cast<parsers::QueryAST*>(result.analyzed_ast.get())) {
        for (auto& col_expr : query_ast->select.columns) {
            SelectNode::ColumnExpr col;
            auto type_it = result.column_types.find(col_expr->to_string());
            col.result_type = (type_it != result.column_types.end()) ? type_it->second : nullptr;
            col.expression = buildExpressionNode(std::move(col_expr));
            node->columns.push_back(std::move(col));
        }

        // Build FROM clause → child TableNode(s)
        for (auto& tbl : query_ast->select.columns) {
            // Use resolved table info if available
            auto it = result.tables.find(tbl->to_string());
            if (it != result.tables.end()) {
                auto tn = buildTableNode(it->second.name, it->second.database);
                node->from = tn;
                break;
            }
        }

        // Build WHERE clause
        if (query_ast->select.where) {
            node->where = buildExpressionNode(std::move(query_ast->select.where));
        }

        // Build GROUP BY
        for (auto& expr : query_ast->select.group_by) {
            node->group_by.push_back(buildExpressionNode(std::move(expr)));
        }

        // Build HAVING
        if (query_ast->select.having) {
            node->having = buildExpressionNode(std::move(query_ast->select.having));
        }

        // Build ORDER BY
        for (auto& ob : query_ast->select.order_by) {
            auto col_ref = std::make_shared<parsers::ASTColumnRef>();
            col_ref->column = ob.first;
            node->order_by.push_back({buildExpressionNode(col_ref),
                                      ob.second});
        }

        // Build LIMIT
        node->limit = query_ast->select.limit;
    }

    return node;
}

auto Analyzer::buildExpressionNode(std::shared_ptr<parsers::ASTExpr> expr)
    -> std::shared_ptr<IQueryTreeNode> {
    // For now, return a minimal SelectNode as a placeholder.
    // Real expression building will iterate ASTExpr subclasses
    // (ASTLiteral, ASTColumnRef, ASTBinaryOp, etc.) and create corresponding IR nodes.
    (void)expr;
    return std::make_shared<SelectNode>();
}

} // namespace mnesso::analyzer
