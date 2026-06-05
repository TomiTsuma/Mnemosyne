// src/Analyzer/analyzer.cpp — Query analyzer: semantic analysis of parsed AST
// Mnemosyne: A column-oriented analytical DBMS

#include "Analyzer/analyzer.h"
#include "Common/exceptions.h"
#include "Functions/function_factory.h"
#include "AggregateFunctions/aggregate_function_factory.h"
#include "DataTypes/data_type_factory.h"
#include <algorithm>
#include <unordered_set>

namespace mnesso::analyzer {

using interpreters::Context;

Analyzer::Analyzer(Context& context) : context_{context} {}

auto Analyzer::analyze(std::shared_ptr<parsers::QueryAST> ast) -> AnalyzeResult {
    AnalyzeResult result;
    result.valid = true;

    switch (ast->query_type) {
        case parsers::QueryAST::QueryType::SELECT: {
            // Resolve table reference
            if (!ast->select.table.empty()) {
                auto storage = context_.get_storage(ast->select.table);
                if (!storage) {
                    result.errors.push_back("Unknown table: " + ast->select.table);
                    result.valid = false;
                    return result;
                }
                result.tables[ast->select.table] = Context::TableInfo{ast->select.table, context_.current_database()};
                
                // Resolve columns from the table
                auto cols = storage->columns();
                auto col_types = storage->column_types();
                for (const auto& col_name : cols) {
                    result.columns[col_name] = Context::ColumnInfo{ast->select.table, ""};
                    if (col_types.find(col_name) != col_types.end()) {
                        result.column_types[col_name] = col_types.at(col_name);
                    }
                }
            }
            
            // Resolve column references in expressions
            for (const auto& col_expr : ast->select.columns) {
                auto expr_errors = resolve_expression_columns(result, col_expr);
                result.errors.insert(result.errors.end(), expr_errors.begin(), expr_errors.end());
            }
            
            if (ast->select.where) {
                auto where_errors = resolve_expression_columns(result, ast->select.where);
                result.errors.insert(result.errors.end(), where_errors.begin(), where_errors.end());
            }
            
            for (const auto& gb_expr : ast->select.group_by) {
                auto gb_errors = resolve_expression_columns(result, gb_expr);
                result.errors.insert(result.errors.end(), gb_errors.begin(), gb_errors.end());
            }
            
            if (ast->select.having) {
                auto having_errors = resolve_expression_columns(result, ast->select.having);
                result.errors.insert(result.errors.end(), having_errors.begin(), having_errors.end());
            }
            
            result.valid = result.errors.empty();
            result.analyzed_ast = ast;
            break;
        }
        case parsers::QueryAST::QueryType::INSERT: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::CREATE: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::DROP: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::SHOW: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::DESCRIBE: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::EXPLAIN: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        default:
            result.errors.push_back("Unknown query type");
            result.valid = false;
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

    // Collect all columns from resolved tables
    for (const auto& [alias, table_info] : result.tables) {
        auto storage = context_.get_storage(table_info.name);
        if (!storage) continue;

        auto cols = storage->columns();
        auto col_types = storage->column_types();
        for (auto& col_name : cols) {
            // Use "table.column" as key for unambiguous resolution
            std::string full_key = table_info.name + "." + col_name;
            result.columns[full_key] = Context::ColumnInfo{table_info.name, col_name};
            if (col_types.find(col_name) != col_types.end()) {
                result.column_types[full_key] = col_types.at(col_name);
            }

            // Also register by column name alone (for simple queries without table prefix)
            if (result.column_types.find(col_name) == result.column_types.end()) {
                result.columns[col_name] = Context::ColumnInfo{table_info.name, col_name};
                if (col_types.find(col_name) != col_types.end()) {
                    result.column_types[col_name] = col_types.at(col_name);
                }
            }
        }
    }

    // If an expression is provided, validate column references within it
    if (expr) {
        errors = resolve_expression_columns(result, expr);
    }

    return errors;
}

// ── resolve_expression_columns — recursively resolve columns in an expression ──

std::vector<std::string> Analyzer::resolve_expression_columns(
    AnalyzeResult& result,
    std::shared_ptr<parsers::ASTExpr> expr) {
    std::vector<std::string> errors;

    if (!expr) return errors;

    // Handle ASTColumnRef
    if (auto* col_ref = dynamic_cast<parsers::ASTColumnRef*>(expr.get())) {
        // Handle "*" wildcard - expand to all columns
        if (col_ref->column == "*") {
            // "*" is valid, no error - it will be expanded during projection
            return errors;
        }

        // Check if column exists
        std::string key = col_ref->table.empty() ? col_ref->column
                                                  : col_ref->table + "." + col_ref->column;

        if (result.column_types.find(key) == result.column_types.end()) {
            // Try just the column name
            if (result.column_types.find(col_ref->column) == result.column_types.end()) {
                errors.push_back("Unknown column: " + key);
                result.unresolved_columns.insert(key);
            }
        }
    }

    // Handle ASTFunction (check arguments)
    if (auto* func = dynamic_cast<parsers::ASTFunction*>(expr.get())) {
        for (auto& arg : func->args) {
            auto arg_errors = resolve_expression_columns(result, arg);
            errors.insert(errors.end(), arg_errors.begin(), arg_errors.end());
        }
    }

    // Handle ASTBinaryOp (check both operands)
    if (auto* binop = dynamic_cast<parsers::ASTBinaryOp*>(expr.get())) {
        auto left_errors = resolve_expression_columns(result, binop->left);
        errors.insert(errors.end(), left_errors.begin(), left_errors.end());

        auto right_errors = resolve_expression_columns(result, binop->right);
        errors.insert(errors.end(), right_errors.begin(), right_errors.end());
    }

    // Handle ASTUnaryOp
    if (auto* unop = dynamic_cast<parsers::ASTUnaryOp*>(expr.get())) {
        auto operand_errors = resolve_expression_columns(result, unop->operand);
        errors.insert(errors.end(), operand_errors.begin(), operand_errors.end());
    }

    // Handle ASTAlias
    if (auto* alias = dynamic_cast<parsers::ASTAlias*>(expr.get())) {
        auto alias_errors = resolve_expression_columns(result, alias->expression);
        errors.insert(errors.end(), alias_errors.begin(), alias_errors.end());
    }

    return errors;
}

auto Analyzer::validate_function(AnalyzeResult& result,
                                 std::shared_ptr<parsers::ASTFunction> func)
    -> std::vector<std::string> {
    std::vector<std::string> errors;

    if (!func) return errors;

    // Check if function exists in factory
    auto& func_factory = functions::FunctionFactory::instance();
    if (!func_factory.has(func->name)) {
        // Check aggregate functions too
        auto& agg_factory = aggregate_functions::AggregateFunctionFactory::instance();
        if (!agg_factory.has(func->name)) {
            errors.push_back("Unknown function: " + func->name);
            return errors;
        }
    }

    (void)result;
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
                // For INSERT, pass the table name
                return buildTableNode(query_ast->insert.table, context_.current_database());
            case parsers::QueryAST::QueryType::CREATE:
                // For CREATE, pass the database or table name
                if (!query_ast->create.database_name.empty()) {
                    return buildTableNode("", query_ast->create.database_name);
                } else {
                    // Extract column names from CREATE TABLE
                    std::vector<std::string> col_names;
                    for (auto& col_def : query_ast->create.columns) {
                        col_names.push_back(col_def.name);
                    }
                    return buildTableNode(query_ast->create.table_name, context_.current_database(), col_names);
                }
            case parsers::QueryAST::QueryType::DROP:
                // For DROP, pass the table name
                return buildTableNode(query_ast->drop.table_name, context_.current_database());
            case parsers::QueryAST::QueryType::SHOW:
                // For SHOW, pass the correct marker based on show type
                if (query_ast->show.show_type == parsers::QueryAST::Show::ShowType::DATABASES) {
                    return buildTableNode("show_databases", context_.current_database());
                } else {
                    return buildTableNode("show_tables", context_.current_database());
                }
            case parsers::QueryAST::QueryType::DESCRIBE:
                // For DESCRIBE, pass the table name
                return buildTableNode(query_ast->describe.table_name, context_.current_database());
            case parsers::QueryAST::QueryType::EXPLAIN:
                // For EXPLAIN, pass unknown for now
                return buildTableNode("unknown", "unknown");
        }
    }

    return nullptr;
}

auto Analyzer::buildTableNode(const std::string& table_name,
                              const std::string& db_name,
                              const std::vector<std::string>& columns)
    -> std::shared_ptr<TableNode> {
    auto node = std::make_shared<TableNode>();
    node->table   = table_name;
    node->database = db_name;
    node->columns = columns;
    return node;
}

auto Analyzer::buildSelectNode(const AnalyzeResult& result)
    -> std::shared_ptr<SelectNode> {
    auto node = std::make_shared<SelectNode>();

    // Build column expressions from the analyzed AST's select list
    if (auto* query_ast = dynamic_cast<parsers::QueryAST*>(result.analyzed_ast.get())) {
        // Build FROM clause → child TableNode
        if (!query_ast->select.table.empty()) {
            auto it = result.tables.find(query_ast->select.table);
            if (it != result.tables.end()) {
                node->from = buildTableNode(it->second.name, it->second.database);
            }
        }

        // Build SELECT columns
        for (auto& col_expr : query_ast->select.columns) {
            SelectNode::ColumnExpr col;
            col.expression = buildExpressionNode(col_expr);
            // Try to infer result type from column_types
            std::string col_name = col_expr->to_string();
            auto type_it = result.column_types.find(col_name);
            col.result_type = (type_it != result.column_types.end()) ? type_it->second : nullptr;
            node->columns.push_back(std::move(col));
        }

        // Build WHERE clause
        if (query_ast->select.where) {
            node->where = buildExpressionNode(query_ast->select.where);
        }

        // Build GROUP BY
        for (auto& expr : query_ast->select.group_by) {
            node->group_by.push_back(buildExpressionNode(expr));
        }

        // Build HAVING
        if (query_ast->select.having) {
            node->having = buildExpressionNode(query_ast->select.having);
        }

        // Build ORDER BY
        for (auto& ob : query_ast->select.order_by) {
            auto col_ref = std::make_shared<parsers::ASTColumnRef>();
            col_ref->column = ob.first;
            node->order_by.push_back({buildExpressionNode(col_ref), ob.second});
        }

        // Build LIMIT
        node->limit = query_ast->select.limit;
    }

    return node;
}

auto Analyzer::buildExpressionNode(std::shared_ptr<parsers::ASTExpr> expr)
    -> std::shared_ptr<IQueryTreeNode> {
    if (!expr) return nullptr;

    // Handle ASTLiteral
    if (auto* literal = dynamic_cast<parsers::ASTLiteral*>(expr.get())) {
        // For literals, we'll create a simple node that holds the value
        // For now, return a placeholder - the planner will handle literal evaluation
        auto node = std::make_shared<SelectNode>();
        return node;
    }

    // Handle ASTColumnRef
    if (auto* col_ref = dynamic_cast<parsers::ASTColumnRef*>(expr.get())) {
        // Column references are handled by the planner - just return a placeholder
        auto node = std::make_shared<SelectNode>();
        return node;
    }

    // Handle ASTFunction
    if (auto* func = dynamic_cast<parsers::ASTFunction*>(expr.get())) {
        // Function calls are handled by the planner
        auto node = std::make_shared<SelectNode>();
        return node;
    }

    // Handle ASTBinaryOp
    if (auto* binop = dynamic_cast<parsers::ASTBinaryOp*>(expr.get())) {
        // Binary operations are handled by the planner
        auto node = std::make_shared<SelectNode>();
        return node;
    }

    // Handle ASTUnaryOp
    if (auto* unop = dynamic_cast<parsers::ASTUnaryOp*>(expr.get())) {
        // Unary operations are handled by the planner
        auto node = std::make_shared<SelectNode>();
        return node;
    }

    // Handle ASTAlias
    if (auto* alias = dynamic_cast<parsers::ASTAlias*>(expr.get())) {
        // Aliases are handled by the planner
        return buildExpressionNode(alias->expression);
    }

    // Default placeholder
    return std::make_shared<SelectNode>();
}

} // namespace mnesso::analyzer
