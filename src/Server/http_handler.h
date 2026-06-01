// src/Server/http_handler.h — HTTP server request handler
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "context.h"
#include "http_types.h"
#include "Parsers/ast.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include "Core/block.h"
#include "Core/series.h"
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>

namespace mnesso::server {

// ── HTTP handler response ──
struct HTTPResponse {
    int status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    static auto ok(std::string body) -> HTTPResponse {
        HTTPResponse resp;
        resp.status_code = 200;
        resp.body = std::move(body);
        resp.headers["Content-Type"] = "text/plain";
        return resp;
    }

    static auto error(int code, std::string body) -> HTTPResponse {
        HTTPResponse resp;
        resp.status_code = code;
        resp.body = std::move(body);
        resp.headers["Content-Type"] = "text/plain";
        return resp;
    }
};

// ── HTTPHandler — routes requests to interpreters ──
class HTTPHandler {
public:
    explicit HTTPHandler(interpreters::Context& context);

    // Handle a request — returns response
    [[nodiscard]] auto handle(const Request& req) -> Response;

    // ── Query handlers ──
    auto handle_query(std::string_view query,
                      std::string_view fmt) -> Response;

    // ── Status ──
    auto handle_ping() -> Response;
    auto handle_status() -> Response;

    // ── Databases ──
    auto handle_databases() -> Response;
    auto handle_tables(std::string_view database) -> Response;
    auto handle_table_schema(std::string_view database, std::string_view table) -> Response;

    // ── System ──
    auto handle_metrics() -> Response;
    auto handle_processors() -> Response;
    auto handle_settings() -> Response;
    auto handle_set(std::string_view key, std::string_view value) -> Response;
    auto handle_kill_query(std::string_view query_id) -> Response;
    auto handle_user_list() -> Response;
    auto handle_roles() -> Response;

private:
    interpreters::Context& context_;

    // ── Query execution ──
    auto execute_query(std::string_view query,
                       std::string_view fmt) -> Response;

    // Common parsing helpers
    auto parse_query(std::string_view sql)
        -> std::variant<parsers::ASTNode, std::string>;
    auto create_ast_block(const parsers::ASTNode& ast)
        -> std::pair<core::Block, std::vector<datatypes::DataTypePtr>>;
    auto execute_ast(const parsers::ASTNode& ast,
                     std::vector<datatypes::DataTypePtr>& column_types)
        -> core::Block;
};

} // namespace mnesso::server
