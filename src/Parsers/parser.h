// src/Parsers/parser.h — Parser base class for recursive descent parsing
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <source_location>

namespace mnemo::parsers {

// ── ParseError — result of a parsing failure ──
struct ParseError {
    std::string message;
    Location    location;

    [[nodiscard]] std::string to_string() const {
        return std::string(message + " at " + location.file + ":" +
                   std::to_string(location.line) + ":" + std::to_string(location.column));
    }
};

// ── Parser — base for all recursive descent parsers ──
class Parser {
public:
    Parser(Lexer lexer);

    // Parse entry point — returns AST or error
    virtual auto parse() -> std::unique_ptr<QueryAST> = 0;

    // Helpers
    auto expect(TokenType type) -> std::optional<Token>;
    auto peek()    const -> Token;
    auto advance() -> Token;
    void error(std::string msg);

    // Current position
    [[nodiscard]] Location current_location() const;

protected:
    // ── Query parsing — override in derived classes ──
    auto parse_query() -> std::unique_ptr<QueryAST>;
    auto parse_use(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_select(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_insert(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_create(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_drop(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_alter(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_if_not_exists() -> bool;
    auto parse_if_exists() -> bool;
    auto parse_show(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_describe(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_explain(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_refresh(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_register(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_drain(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_remove(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_test(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_discover(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_run(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_pause(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_resume(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_publish(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_subscribe(std::unique_ptr<QueryAST>& ast) -> void;
    auto consume_pipeline_keyword() -> bool;
    auto consume_stream_keyword() -> bool;
    auto consume_topic_keyword() -> bool;
    auto consume_consumer_group_keyword() -> bool;
    auto parse_retention_clause(QueryAST::Create& create) -> void;
    auto parse_topic_properties(QueryAST::Create& create) -> void;
    auto consume_stage_keyword() -> bool;
    auto consume_task_keyword() -> bool;
    auto consume_trigger_keyword() -> bool;
    auto parse_pipeline_owner(QueryAST::Create& create) -> void;
    auto parse_stage_order(QueryAST::Create& create) -> void;
    auto parse_task_properties(QueryAST::Create& create) -> void;
    auto parse_trigger_properties(QueryAST::Create& create) -> void;
    auto parse_in_pipeline_clause(std::string& pipeline_name) -> void;
    auto parse_storage_unit_properties(QueryAST::Create& create) -> void;
    auto parse_connector_properties(QueryAST::Create& create) -> void;
    auto consume_connector_keyword() -> bool;
    auto parse_node_properties(QueryAST::Create& create) -> void;
    auto parse_property_value() -> std::string;
    auto consume_storage_unit_keyword() -> bool;
    auto consume_node_keyword() -> bool;
    auto consume_replica_group_keyword() -> bool;
    auto parse_replica_group_properties(QueryAST::Create& create) -> void;
    auto consume_shard_group_keyword() -> bool;
    auto parse_shard_group_properties(QueryAST::Create& create) -> void;

    // ── MODEL layer ──
    auto parse_model_clauses(QueryAST::Create& create) -> void;
    auto parse_feature_set_clauses(QueryAST::Create& create) -> void;
    auto parse_kv_list(std::unordered_map<std::string, std::string>& out) -> void;
    auto parse_paren_kv_pairs(std::vector<std::pair<std::string, std::string>>& out) -> void;
    auto parse_deploy(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_predict(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_evaluate(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_compare(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_generate(std::unique_ptr<QueryAST>& ast) -> void;
    auto parse_model_version_ref(std::string& model, uint32_t& version) -> void;

    // ── Expression parsing helpers ──
    auto parse_expression() -> std::shared_ptr<ASTExpr>;
    auto parse_comparison() -> std::shared_ptr<ASTExpr>;
    auto parse_term() -> std::shared_ptr<ASTExpr>;
    auto parse_factor() -> std::shared_ptr<ASTExpr>;
    auto parse_expression_list() -> std::vector<std::shared_ptr<ASTExpr>>;

    // ── Clause parsing helpers ──
    auto parse_table_name() -> std::string;
    auto parse_name_or_keyword() -> std::string;
    auto parse_table_ref() -> std::pair<std::string, std::string>;
    auto parse_subquery() -> std::shared_ptr<QueryAST>;
    auto parse_window_spec(ASTFunction::WindowSpec& spec) -> void;
    auto parse_database_name() -> std::string;
    auto parse_column_list() -> std::vector<std::string>;
    auto parse_column_definitions() -> std::vector<ColumnDef>;
    auto parse_value_list() -> std::vector<std::vector<std::string>>;
    auto parse_value_row() -> std::vector<std::string>;
    auto parse_order_by_list() -> std::vector<OrderBy>;
    auto parse_order_by() -> OrderBy;
    auto parse_limit() -> std::pair<size_t, size_t>;

    // ── Token helpers ──
    auto consume() -> void;
    [[nodiscard]] auto is_current(TokenType type) const -> bool;

    Lexer lexer_;
    Token  current_;
    bool   has_error_ = false;
};

// ── ParserResult — result of a parsing operation ──
template<typename T>
using ParseResult = std::variant<std::shared_ptr<T>, ParseError>;

} // namespace mnemo::parsers
