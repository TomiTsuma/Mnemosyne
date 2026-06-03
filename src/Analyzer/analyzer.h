// src/Analyzer/analyzer.h — Query analyzer: semantic analysis of parsed AST
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "ast.h"
#include "context.h"
#include "query_tree.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mnesso::analyzer {

using interpreters::Context;

// ── ColumnRef — resolved reference to a column by name ──
struct ColumnRef {
    std::string column;
    ColumnRef() = default;
    explicit ColumnRef(std::string col) : column(std::move(col)) {}
};

// ── AnalyzeResult — outcome of semantic analysis ──
struct AnalyzeResult {
    std::shared_ptr<parsers::ASTNode>         analyzed_ast;
    std::unordered_map<std::string, Context::TableInfo> tables;
    std::unordered_map<std::string, Context::ColumnInfo> columns;
    std::unordered_map<std::string, datatypes::DataTypePtr> column_types;
    std::unordered_set<std::string>             unresolved_columns;
    std::vector<std::string>                    errors;
    bool                                        valid;
};

// ── Analyzer — semantic analysis pass ──
class Analyzer {
public:
    explicit Analyzer(Context& context);

    // Main entry — analyze a parsed AST and return result
    auto analyze(std::shared_ptr<parsers::QueryAST> ast) -> AnalyzeResult;

    // Resolve table references
    auto resolve_tables(AnalyzeResult& result,
                        std::shared_ptr<parsers::ASTFromClause> from)
        -> std::vector<std::string>;

    // Resolve column references against resolved tables
    auto resolve_columns(AnalyzeResult& result,
                         std::shared_ptr<parsers::ASTExpr> expr)
        -> std::vector<std::string>;

    // Resolve columns recursively in expressions
    auto resolve_expression_columns(AnalyzeResult& result,
                                     std::shared_ptr<parsers::ASTExpr> expr)
        -> std::vector<std::string>;

    // Validate function arguments
    auto validate_function(AnalyzeResult& result,
                           std::shared_ptr<parsers::ASTFunction> func)
        -> std::vector<std::string>;

    // Check for errors (returns true if analysis is valid)
    [[nodiscard]] auto has_errors(const AnalyzeResult& result) const -> bool;

    // Convert analyzed AST + metadata into an IQueryTreeNode IR
    auto buildQueryTree(AnalyzeResult& result) -> std::shared_ptr<IQueryTreeNode>;

private:
    Context& context_;

    // ── Internal helpers for buildQueryTree ──
    auto buildSelectNode(const AnalyzeResult& result)
        -> std::shared_ptr<SelectNode>;
    auto buildTableNode(const std::string& table_name,
                        const std::string& db_name,
                        const std::vector<std::string>& columns = {})
        -> std::shared_ptr<TableNode>;
    auto buildExpressionNode(std::shared_ptr<parsers::ASTExpr> expr)
        -> std::shared_ptr<IQueryTreeNode>;
};

} // namespace mnesso::analyzer
