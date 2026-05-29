// src/Server/http_handler.h — HTTP server request handler
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "context.h"
#include "server.h"
#include "parsers/ast.h"
#include "parsers/lexer.h"
#include "parsers/parser_query.h"
#include "core/block.h"
#include "core/series.h"
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
    explicit HTTPHandler(Context& context);

    // Handle a request — returns response
    [[nodiscard]] auto handle(std::string_view method,
                              std::string_view path,
                              std::string_view body) -> HTTPResponse;

    // ── Query handlers ──
    auto handle_query(std::string_view query,
                      std::string format = "JSON") -> HTTPResponse;

    // ── Status ──
    auto handle_ping() -> HTTPResponse;
    auto handle_status() -> HTTPResponse;

    // ── Databases ──
    auto handle_databases() -> HTTPResponse;
    auto handle_tables(std::string_view database) -> HTTPResponse;
    auto handle_table_schema(std::string_view database, std::string_view table) -> HTTPResponse;

    // ── System ──
    auto handle_metrics() -> HTTPResponse;
    auto handle_processors() -> HTTPResponse;
    auto handle_settings() -> HTTPResponse;
    auto handle_set(std::string_view key, std::string_view value) -> HTTPResponse;
    auto handle_kill_query(std::string_view query_id) -> HTTPResponse;
    auto handle_user_list() -> HTTPResponse;
    auto handle_roles() -> HTTPResponse;

private:
    Context& context_;

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
