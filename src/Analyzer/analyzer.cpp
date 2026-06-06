// src/Analyzer/analyzer.cpp — Query analyzer: semantic analysis of parsed AST
// Mnemosyne: A column-oriented analytical DBMS

#include "Analyzer/analyzer.h"
#include "Databases/database.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Common/exceptions.h"
#include "Functions/function_factory.h"
#include "AggregateFunctions/aggregate_function_factory.h"
#include "DataTypes/data_type_factory.h"
#include <algorithm>
#include <unordered_set>

namespace mnemo::analyzer {

using interpreters::Context;

Analyzer::Analyzer(Context& context) : context_{context} {}

auto Analyzer::analyze(std::shared_ptr<parsers::QueryAST> ast) -> AnalyzeResult {
    AnalyzeResult result;
    result.valid = true;

    switch (ast->query_type) {
        case parsers::QueryAST::QueryType::SELECT: {
            auto register_table = [&](const std::string& table, const std::string& alias) {
                auto storage = interpreters::ddl_utils::resolve_storage(context_, table);
                if (!storage) {
                    const auto db_name = interpreters::ddl_utils::resolve_current_database(context_);
                    if (auto idb = context_.get_database(db_name)) {
                        if (auto catalog = std::dynamic_pointer_cast<databases::Database>(idb)) {
                            if (catalog->has_view(table)) {
                                auto view = catalog->get_view(table).value();
                                parsers::QueryAST nested;
                                nested.query_type = parsers::QueryAST::QueryType::SELECT;
                                nested.select = view.definition;
                                auto nested_result = analyze(std::make_shared<parsers::QueryAST>(nested));
                                if (!nested_result.valid) {
                                    for (const auto& err : nested_result.errors) {
                                        result.errors.push_back(err);
                                    }
                                    return;
                                }
                                const std::string key = alias.empty() ? table : alias;
                                result.tables[key] = Context::TableInfo{table, db_name};
                                for (const auto& [col_key, col_info] : nested_result.columns) {
                                    result.columns[col_key] = col_info;
                                }
                                for (const auto& [col_key, col_type] : nested_result.column_types) {
                                    result.column_types[col_key] = col_type;
                                }
                                return;
                            }
                        }
                    }
                    result.errors.push_back("Unknown table: " + table);
                    return;
                }
                const std::string key = alias.empty() ? table : alias;
                result.tables[key] = Context::TableInfo{table, context_.current_database()};

                auto cols = storage->columns();
                auto col_types = storage->column_types();
                for (const auto& col_name : cols) {
                    const std::string full_key = table + "." + col_name;
                    result.columns[full_key] = Context::ColumnInfo{table, col_name};
                    if (col_types.contains(col_name)) {
                        result.column_types[full_key] = col_types.at(col_name);
                    }
                    if (!result.column_types.contains(col_name)) {
                        result.columns[col_name] = Context::ColumnInfo{table, col_name};
                        if (col_types.contains(col_name)) {
                            result.column_types[col_name] = col_types.at(col_name);
                        }
                    }
                    const std::string alias_key = key + "." + col_name;
                    result.columns[alias_key] = Context::ColumnInfo{table, col_name};
                    if (col_types.contains(col_name)) {
                        result.column_types[alias_key] = col_types.at(col_name);
                    }
                }
            };

            if (!ast->select.table.empty()) {
                register_table(ast->select.table,
                    ast->select.table_alias.empty() ? ast->select.table : ast->select.table_alias);
            }
            for (const auto& join : ast->select.joins) {
                register_table(join.table, join.alias.empty() ? join.table : join.alias);
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

            for (const auto& join : ast->select.joins) {
                if (join.on) {
                    auto on_errors = resolve_expression_columns(result, join.on);
                    result.errors.insert(result.errors.end(), on_errors.begin(), on_errors.end());
                }
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
        case parsers::QueryAST::QueryType::ALTER: {
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
        case parsers::QueryAST::QueryType::USE: {
            // USE is a simple session-context statement: no column/table
            // resolution is required beyond parsing (mirrors CREATE DATABASE).
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::REFRESH: {
            result.analyzed_ast = ast;
            result.valid = true;
            break;
        }
        case parsers::QueryAST::QueryType::REGISTER:
        case parsers::QueryAST::QueryType::DRAIN:
        case parsers::QueryAST::QueryType::REMOVE: {
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
        if (col_ref->column == "*") {
            return errors;
        }

        std::string table_name;
        if (!col_ref->table.empty()) {
            auto it = result.tables.find(col_ref->table);
            table_name = (it != result.tables.end()) ? it->second.name : col_ref->table;
        }

        const std::string key = table_name.empty()
            ? col_ref->column
            : table_name + "." + col_ref->column;
        const std::string alias_key = col_ref->table.empty()
            ? col_ref->column
            : col_ref->table + "." + col_ref->column;

        if (!result.column_types.contains(key) &&
            !result.column_types.contains(alias_key) &&
            !result.column_types.contains(col_ref->column)) {
            errors.push_back("Unknown column: " +
                (col_ref->table.empty() ? col_ref->column : col_ref->table + "." + col_ref->column));
            result.unresolved_columns.insert(key);
        }
    }

    if (dynamic_cast<parsers::ASTSubQueryExpr*>(expr.get())) {
        return errors;
    }

    // Handle ASTFunction (check arguments)
    if (auto* func = dynamic_cast<parsers::ASTFunction*>(expr.get())) {
        std::string upper = func->name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        static const std::unordered_set<std::string> builtins = {
            "COUNT", "SUM", "AVG", "MIN", "MAX"};
        if (builtins.count(upper)) {
            for (auto& arg : func->args) {
                if (auto* col = dynamic_cast<parsers::ASTColumnRef*>(arg.get())) {
                    if (col->column == "*") continue;
                }
                auto arg_errors = resolve_expression_columns(result, arg);
                errors.insert(errors.end(), arg_errors.begin(), arg_errors.end());
            }
            if (func->window) {
                for (auto& part : func->window->partition_by) {
                    auto pe = resolve_expression_columns(result, part);
                    errors.insert(errors.end(), pe.begin(), pe.end());
                }
            }
            return errors;
        }
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
            case parsers::QueryAST::QueryType::CREATE:
            case parsers::QueryAST::QueryType::DROP:
            case parsers::QueryAST::QueryType::ALTER:
            case parsers::QueryAST::QueryType::SHOW:
            case parsers::QueryAST::QueryType::DESCRIBE:
            case parsers::QueryAST::QueryType::EXPLAIN:
            case parsers::QueryAST::QueryType::USE:
            case parsers::QueryAST::QueryType::REFRESH:
                return buildDDLNode(*query_ast);
        }
    }

    return nullptr;
}

auto Analyzer::buildDDLNode(const parsers::QueryAST& query_ast)
    -> std::shared_ptr<DDLNode> {
    auto node = std::make_shared<DDLNode>();

    switch (query_ast.query_type) {
        case parsers::QueryAST::QueryType::INSERT:
            node->kind = DDLNode::Kind::Insert;
            node->table = query_ast.insert.table;
            node->database = context_.current_database();
            node->insert_columns = query_ast.insert.columns;
            node->insert_values = query_ast.insert.values;
            break;
        case parsers::QueryAST::QueryType::CREATE:
            if (query_ast.create.kind == parsers::QueryAST::Create::Kind::Database) {
                node->kind         = DDLNode::Kind::CreateDatabase;
                node->database     = query_ast.create.database_name;
                node->if_not_exists = query_ast.create.if_not_exists;
            } else {
                node->kind          = DDLNode::Kind::CreateTable;
                node->table         = query_ast.create.table_name;
                node->database      = context_.current_database();
                node->if_not_exists = query_ast.create.if_not_exists;
                node->engine        = query_ast.create.engine;
                node->column_defs   = query_ast.create.columns;
            }
            break;
        case parsers::QueryAST::QueryType::DROP:
            node->table     = query_ast.drop.table;
            node->database  = context_.current_database();
            node->if_exists = query_ast.drop.if_exists;
            switch (query_ast.drop.kind) {
                case parsers::QueryAST::Drop::Kind::Truncate:
                    node->kind = DDLNode::Kind::Truncate;
                    break;
                case parsers::QueryAST::Drop::Kind::Detach:
                    node->kind = DDLNode::Kind::Detach;
                    break;
                default:
                    node->kind = DDLNode::Kind::Drop;
                    break;
            }
            break;
        case parsers::QueryAST::QueryType::ALTER:
            node->kind           = DDLNode::Kind::Alter;
            node->table          = query_ast.alter.table;
            node->database       = context_.current_database();
            node->alter_commands = query_ast.alter.commands;
            break;
        case parsers::QueryAST::QueryType::SHOW:
            switch (query_ast.show.show_type) {
                case parsers::QueryAST::Show::ShowType::DATABASES:
                    node->kind = DDLNode::Kind::ShowDatabases;
                    break;
                case parsers::QueryAST::Show::ShowType::VIEWS:
                    node->kind = DDLNode::Kind::ShowViews;
                    break;
                case parsers::QueryAST::Show::ShowType::MATERIALIZED_VIEWS:
                    node->kind = DDLNode::Kind::ShowMaterializedViews;
                    break;
                case parsers::QueryAST::Show::ShowType::STORAGE_UNITS:
                    node->kind = DDLNode::Kind::ShowStorageUnits;
                    break;
                case parsers::QueryAST::Show::ShowType::STORAGE_USAGE:
                    node->kind = DDLNode::Kind::ShowStorageUsage;
                    break;
                case parsers::QueryAST::Show::ShowType::NODES:
                    node->kind = DDLNode::Kind::ShowNodes;
                    break;
                case parsers::QueryAST::Show::ShowType::NODE_METRICS:
                    node->kind = DDLNode::Kind::ShowNodeMetrics;
                    break;
                case parsers::QueryAST::Show::ShowType::NODE_CAPABILITIES:
                    node->kind = DDLNode::Kind::ShowNodeCapabilities;
                    break;
                case parsers::QueryAST::Show::ShowType::NODE_PARTITIONS:
                    node->kind = DDLNode::Kind::ShowNodePartitions;
                    break;
                case parsers::QueryAST::Show::ShowType::NODE_REPLICAS:
                    node->kind = DDLNode::Kind::ShowNodeReplicas;
                    break;
                case parsers::QueryAST::Show::ShowType::CLUSTERS:
                    node->kind = DDLNode::Kind::ShowClusters;
                    break;
                case parsers::QueryAST::Show::ShowType::TABLES:
                default:
                    node->kind = DDLNode::Kind::ShowTables;
                    break;
            }
            node->show_node_name = query_ast.show.node_name;
            node->database = context_.current_database();
            break;
        case parsers::QueryAST::QueryType::DESCRIBE:
            node->kind          = DDLNode::Kind::Describe;
            node->table         = query_ast.describe.table_name;
            node->database      = context_.current_database();
            node->describe_kind = query_ast.describe.object_kind;
            break;
        case parsers::QueryAST::QueryType::EXPLAIN:
            node->kind = DDLNode::Kind::Explain;
            break;
        case parsers::QueryAST::QueryType::USE:
            node->kind         = DDLNode::Kind::Use;
            node->use_database = query_ast.use.database_name;
            break;
        case parsers::QueryAST::QueryType::REFRESH:
            node->kind         = DDLNode::Kind::Refresh;
            node->refresh_name = query_ast.refresh.name;
            break;
        default:
            break;
    }

    return node;
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

} // namespace mnemo::analyzer
