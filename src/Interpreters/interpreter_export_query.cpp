// src/Interpreters/interpreter_export_query.cpp — EXPORT TABLE ... TO CONNECTOR

#include "Interpreters/interpreter_export_query.h"
#include "Interpreters/interpreter_select_query.h"
#include "Columns/column_string.h"
#include "Columns/column_vector.h"
#include "Connectors/connector_factory.h"
#include "Connectors/connector_manager.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::interpreters {

namespace {

auto string_column(const std::string& value) -> core::ColumnPtr {
    auto column = std::make_shared<columns::ColumnString>();
    column->insert(core::Field{value});
    return column;
}

} // namespace

auto InterpreterExportQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    const auto& request = query.export_data;
    const auto mode =
        connectors::parse_export_mode(request.mode.empty() ? "APPEND" : request.mode);

    const auto* entry = connectors::ConnectorManager::instance().get_connector(request.connector);
    if (!entry) {
        throw common::Exception{
            "Unknown connector: " + request.connector,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    const auto& capabilities = entry->capabilities;
    if (std::find(capabilities.begin(), capabilities.end(), "WRITE") == capabilities.end()) {
        throw common::Exception{
            "Connector " + request.connector + " does not support WRITE",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    // "db.table" exports to a resource named after the table alone.
    auto resource = request.resource;
    if (resource.empty()) {
        const auto dot = request.table.rfind('.');
        resource = dot == std::string::npos ? request.table : request.table.substr(dot + 1);
    }

    // Read through SELECT so tables, views and materialized views are all exportable. The name
    // goes into the AST directly, never into SQL text.
    parsers::Lexer lexer{std::string{"SELECT * FROM export_source"}};
    parsers::QueryParser parser{std::move(lexer)};
    auto select = parser.parse();
    select->select.table = request.table;
    const auto data = InterpreterSelectQuery::execute(context, *select);

    auto driver = connectors::ConnectorFactory::create_driver(entry->type);
    const auto rows = driver->write(*entry, resource, data, mode);

    auto row_count = std::make_shared<columns::ColumnVector<int64_t>>();
    row_count->insert(core::Field{static_cast<int64_t>(rows)});

    core::Block result;
    result.add_column("connector", string_column(request.connector));
    result.add_column("resource", string_column(resource));
    result.add_column("mode", string_column(connectors::export_mode_name(mode)));
    result.add_column("rows", row_count);
    return result;
}

} // namespace mnemo::interpreters
