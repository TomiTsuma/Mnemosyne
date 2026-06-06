// src/Server/http_handler.cpp — HTTP request handler
// Mnemosyne: A column-oriented analytical DBMS

#include "Server/http_handler.h"
#include "Server/http_server.h"
#include "Interpreters/query_executor.h"
#include "Interpreters/blockInterpreter.h"
#include "Interpreters/interpreter_select_query.h"
#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/interpreter_insert_query.h"
#include "Interpreters/interpreter_drop_query.h"
#include "Interpreters/interpreter_refresh_query.h"
#include "Interpreters/context.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include <iostream>
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Common/exceptions.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>

namespace mnemo::server {

// ── HTTPHandler ──

HTTPHandler::HTTPHandler(interpreters::Context& context)
    : context_(context) {}

auto HTTPHandler::handle(const Request& req) -> Response {
    // Route based on path
    if (req.path == "/ping") {
        return handle_ping();
    } else if (req.path == "/") {
        return handle_index();
    } else if (req.path == "/status") {
        return handle_status();
    } else if (req.path == "/databases") {
        return handle_databases();
    } else if (req.path.rfind("/tables/", 0) == 0) {
        auto db = req.path.substr(8); // skip "/tables/"
        return handle_tables(db);
    } else if (req.path.rfind("/schema/", 0) == 0) {
        auto parts = req.path.substr(8); // skip "/schema/"
        auto slash = parts.find('/');
        if (slash != std::string::npos) {
            auto db = parts.substr(0, slash);
            auto tbl = parts.substr(slash + 1);
            return handle_table_schema(db, tbl);
        }
        return Response::error(400, "Bad request: /schema/{db}/{table}");
    } else if (req.path == "/metrics") {
        return handle_metrics();
    } else if (req.path == "/settings") {
        return handle_settings();
    } else if (req.path == "/query") {
        // Execute query from query params or body
        if (req.query_params.find("query") == req.query_params.end() && req.body.empty()) {
            return Response::error(400, "Missing 'query' parameter");
        }
        auto query_it = req.query_params.find("query");
        std::string query = (query_it != req.query_params.end()) ? query_it->second : req.body;
        auto fmt_it = req.query_params.find("format");
        std::string fmt = (fmt_it != req.query_params.end()) ? fmt_it->second : "JSON";
        return handle_query(query, fmt);
    } else {
        return Response::error(404, "Not found: " + req.path);
    }
}

auto HTTPHandler::handle_ping() -> Response {
    return Response::ok("Ok.\n");
}

auto HTTPHandler::handle_status() -> Response {
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    oss << "Uptime: running\n"
        << "Version: 0.1.0\n"
        << "Time: " << std::ctime(&t)
        << "Threads: " << context_.pool().active_count() << "\n";
    return Response::ok(oss.str());
}

auto HTTPHandler::handle_index() -> Response {
    // Serve the web UI from public/index.html
    std::ifstream file("public/index.html");
    if (!file.is_open()) {
        return Response::error(500, "UI file not found: public/index.html");
    }
    std::string html{std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>()};
    return Response::html(std::move(html));
}

auto HTTPHandler::handle_databases() -> Response {
    auto dbs = context_.databases();
    std::ostringstream oss;
    oss << "{\n  \"databases\": [\n";
    for (size_t i = 0; i < dbs.size(); ++i) {
        if (i > 0) oss << ",\n";
        oss << "    \"" << dbs[i] << "\"";
    }
    oss << "\n  ]\n}\n";
    return Response::json(oss.str());
}

auto HTTPHandler::handle_tables(std::string_view database) -> Response {
    auto db = context_.get_database(database);
    if (!db) {
        return Response::error(404, "Database not found: " + std::string(database));
    }
    auto tables = db->tables();
    std::ostringstream oss;
    oss << "{\n  \"database\": \"" << database << "\",\n  \"tables\": [\n";
    for (size_t i = 0; i < tables.size(); ++i) {
        if (i > 0) oss << ",\n";
        oss << "    \"" << tables[i] << "\"";
    }
    oss << "\n  ]\n}\n";
    return Response::json(oss.str());
}

auto HTTPHandler::handle_table_schema(std::string_view database, std::string_view table) -> Response {
    auto db = context_.get_database(database);
    if (!db) {
        return Response::error(404, "Database not found: " + std::string(database));
    }
    auto storage = db->table(std::string{table});
    if (!storage) {
        return Response::error(404, "Table not found: " + std::string(table));
    }
    auto cols = storage->columns();
    auto col_types = storage->column_types();
    std::ostringstream oss;
    oss << "{\n  \"database\": \"" << database << "\",\n"
        << "  \"table\": \"" << table << "\",\n"
        << "  \"columns\": [\n";
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i > 0) oss << ",\n";
        auto it = col_types.find(cols[i]);
        std::string type_str = it != col_types.end() ? it->second->name() : "Unknown";
        oss << "    {\"name\": \"" << cols[i] << "\", \"type\": \"" << type_str << "\"}";
    }
    oss << "\n  ]\n}\n";
    return Response::json(oss.str());
}

auto HTTPHandler::handle_metrics() -> Response {
    auto current_db = context_.current_database();
    std::ostringstream oss;
    oss << "{\n"
        << "  \"queries_total\": 0,\n"
        << "  \"queries_failed\": 0,\n"
        << "  \"bytes_read\": 0,\n"
        << "  \"bytes_written\": 0,\n"
        << "  \"memory_tracked\": " << context_.total_memory() << ",\n"
        << "  \"current_database\": \"" << current_db << "\"\n"
        << "}\n";
    return Response::json(oss.str());
}

auto HTTPHandler::handle_settings() -> Response {
    auto settings = context_.get_settings();
    auto names = settings.names();
    std::ostringstream oss;
    oss << "{\n  \"settings\": {\n";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ",\n";
        auto val = settings.get(names[i]);
        oss << "    \"" << names[i] << "\": ";
        if (val) {
            std::visit([&oss](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) oss << (v ? "true" : "false");
                else if constexpr (std::is_integral_v<T>) oss << v;
                else if constexpr (std::is_floating_point_v<T>) oss << v;
                else oss << "\"" << std::string(std::begin(v), std::end(v)) << "\"";
            }, *val);
        } else {
            oss << "null";
        }
    }
    oss << "\n  }\n}\n";
    return Response::json(oss.str());
}

auto HTTPHandler::handle_query(std::string_view query, std::string_view fmt) -> Response {
    // Debug: print query bytes
    std::cerr << "[DEBUG] Query length: " << query.size() << std::endl;
    for (size_t i = 0; i < query.size(); ++i) {
        std::cerr << "  [" << i << "] = 0x" << std::hex << (int)(unsigned char)query[i] << " '" << (query[i] >= 32 && query[i] < 127 ? query[i] : '?') << "'" << std::dec << std::endl;
    }
    return execute_query(query, fmt);
}

auto HTTPHandler::execute_query(std::string_view query, std::string_view fmt) -> Response {
    try {
        auto start = std::chrono::steady_clock::now();

        // Parse
        parsers::Lexer lexer{std::string{query}};
        parsers::QueryParser parser{std::move(lexer)};
        auto ast = parser.parse();

        if (!ast) {
            return Response::error_json(400, "Parse error: Invalid SQL");
        }

        // Analyze
        analyzer::Analyzer analyzer{context_};
        auto result = analyzer.analyze(std::shared_ptr<parsers::QueryAST>{std::move(ast)});

        if (analyzer.has_errors(result)) {
            std::string errors;
            for (auto& e : result.errors) {
                errors += e + "\n";
            }
            return Response::error_json(400, "Analysis error: " + errors);
        }

        interpreters::QueryResult query_result;
        if (auto* query_ast = dynamic_cast<parsers::QueryAST*>(result.analyzed_ast.get())) {
            switch (query_ast->query_type) {
                case parsers::QueryAST::QueryType::SELECT: {
                    auto block = interpreters::InterpreterSelectQuery::execute(context_, *query_ast);
                    query_result.block = std::make_shared<core::Block>(std::move(block));
                    break;
                }
                case parsers::QueryAST::QueryType::INSERT: {
                    auto block = interpreters::InterpreterInsertQuery::execute(context_, *query_ast);
                    query_result.block = std::make_shared<core::Block>(std::move(block));
                    break;
                }
                case parsers::QueryAST::QueryType::CREATE: {
                    auto block = interpreters::InterpreterCreateQuery::execute(context_, *query_ast);
                    query_result.block = std::make_shared<core::Block>(std::move(block));
                    break;
                }
                case parsers::QueryAST::QueryType::DROP: {
                    auto block = interpreters::InterpreterDropQuery::execute(context_, *query_ast);
                    query_result.block = std::make_shared<core::Block>(std::move(block));
                    break;
                }
                case parsers::QueryAST::QueryType::REFRESH: {
                    auto block = interpreters::InterpreterRefreshQuery::execute(context_, *query_ast);
                    query_result.block = std::make_shared<core::Block>(std::move(block));
                    break;
                }
                default:
                    break;
            }
        }
        if (!query_result.block) {
            auto query_tree = analyzer.buildQueryTree(result);
            planner::Planner planner{context_};
            auto plan = planner.plan(query_tree);
            auto executor = interpreters::InterpreterFactory::create(plan, context_);
            query_result = executor->execute();
        }

        auto end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (!query_result.error.empty()) {
            return Response::error_json(500, "Execution error: " + query_result.error);
        }

        // Format output
        if (fmt == "JSON") {
            std::cerr << "[DEBUG] JSON serialization: row_count=" << query_result.block->row_count() 
                      << ", column_count=" << query_result.block->column_count() << std::endl;
            std::ostringstream oss;
            oss << "{\n"
                << "  \"rows\": " << query_result.block->row_count() << ",\n"
                << "  \"columns\": [\n";
            auto col_names = query_result.block->column_names();
            for (size_t i = 0; i < col_names.size(); ++i) {
                if (i > 0) oss << ",\n";
                oss << "    \"" << col_names[i] << "\"";
            }
            oss << "\n  ],\n"
                << "  \"data\": [\n";

            // Output first few rows
            size_t max_rows = (query_result.block->row_count() < size_t{100})
                ? query_result.block->row_count() : size_t{100};
            for (size_t r = 0; r < max_rows; ++r) {
                if (r > 0) oss << ",\n";
                oss << "    [";
                for (size_t c = 0; c < col_names.size(); ++c) {
                    if (c > 0) oss << ", ";
                    auto field = query_result.block->get_row_value(c, r);
                    std::visit([&](auto&& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::monostate>) oss << "null";
                        else if constexpr (std::is_integral_v<T>) oss << v;
                        else if constexpr (std::is_floating_point_v<T>) oss << v;
                        else if constexpr (std::is_same_v<T, bool>) oss << (v ? "true" : "false");
                        else if constexpr (std::is_same_v<T, std::shared_ptr<void>>) {
                            if (v) oss << "<ptr:" << v.get() << ">";
                            else   oss << "null";
                        }
                        else oss << "\"" << std::string(std::begin(v), std::end(v)) << "\"";
                    }, field.variant());
                }
                oss << "]";
            }
            if (query_result.block->row_count() > max_rows) {
                oss << ",\n    \"... (" << (query_result.block->row_count() - max_rows) << " more rows)\"";
            }
            oss << "\n  ],\n"
                << "  \"duration_ms\": " << std::fixed << std::setprecision(2) << elapsed_ms << ",\n"
                << "  \"bytes_read\": " << query_result.bytes_read << ",\n"
                << "  \"bytes_written\": " << query_result.bytes_written << "\n"
                << "}\n";
            return Response::json(oss.str());
        } else {
            // Tabular format
            std::ostringstream oss;
            auto col_names = query_result.block->column_names();
            // Header
            for (auto& name : col_names) {
                oss << name << "\t";
            }
            oss << "\n";
            // Rows
            size_t max_rows = (query_result.block->row_count() < size_t{1000})
                ? query_result.block->row_count() : size_t{1000};
            for (size_t r = 0; r < max_rows; ++r) {
                for (size_t c = 0; c < col_names.size(); ++c) {
                    if (c > 0) oss << "\t";
                    auto field = query_result.block->get_row_value(c, r);
                    std::visit([&](auto&& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::monostate>) oss << "\\N";
                        else if constexpr (std::is_integral_v<T>) oss << v;
                        else if constexpr (std::is_floating_point_v<T>) oss << v;
                        else if constexpr (std::is_same_v<T, bool>) oss << (v ? "true" : "false");
                        else if constexpr (std::is_same_v<T, std::shared_ptr<void>>) {
                            if (v) oss << "<ptr:" << v.get() << ">";
                            else   oss << "\\N";
                        }
                        else oss << std::string(std::begin(v), std::end(v));
                    }, field.variant());
                }
                oss << "\n";
            }
            if (query_result.block->row_count() > max_rows) {
                oss << "... (" << (query_result.block->row_count() - max_rows) << " more rows)\n";
            }
            oss << query_result.rows_read << " rows in "
                << std::fixed << std::setprecision(2) << elapsed_ms << " ms\n";
            return Response::ok(oss.str());
        }
    } catch (const common::Exception& e) {
        return Response::error_json(500, "Exception: " + std::string{e.what()});
    } catch (const std::exception& e) {
        return Response::error_json(500, "Error: " + std::string{e.what()});
    } catch (...) {
        return Response::error_json(500, "Unknown error");
    }
}

} // namespace mnemo::server
