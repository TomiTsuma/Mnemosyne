// src/Interpreters/interpreter_insert_query.cpp — INSERT interpreter implementation

#include "Interpreters/interpreter_insert_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_select_query.h"
#include "Streaming/stream_manager.h"
#include "Pipelines/pipeline_event_bus.h"
#include "Core/field.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace mnemo::interpreters {

namespace {

auto strip_quotes(std::string value) -> std::string {
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

auto insert_typed_value(core::IColumn& col,
                        const datatypes::DataTypePtr& type,
                        const std::string& raw_value) -> void {
    const auto value = strip_quotes(raw_value);
    const auto type_name = type->name();

    if (type_name == "Int64" || type_name == "UInt64" || type_name == "Int32") {
        col.insert(core::Field(static_cast<int64_t>(std::stoll(value))));
    } else if (type_name == "Float64" || type_name == "Float32") {
        col.insert(core::Field(std::stod(value)));
    } else if (type_name == "UInt8" || type_name == "Bool") {
        col.insert(core::Field(value == "1" || value == "true" || value == "TRUE"));
    } else {
        col.insert(core::Field(value));
    }
}

// ── INSERT ... SELECT support ──
// Columns produced by a SELECT (or a connector scan) carry whatever runtime type the
// expression yielded — frequently text, since connectors return JSON strings. These helpers
// convert each value to the target column's declared type, refusing a value that does not fit
// rather than silently truncating it, so a load is all-or-nothing.

auto is_integer_type(const std::string& type_name) -> bool {
    return type_name == "Int64" || type_name == "Int32" || type_name == "Int16"
        || type_name == "Int8" || type_name == "UInt64" || type_name == "UInt32"
        || type_name == "UInt16";
}

auto trim_ws(std::string value) -> std::string {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

auto field_display(const core::Field& value) -> std::string {
    if (auto s = value.as_string()) return *s;
    if (value.type() == core::FieldType::Float32 || value.type() == core::FieldType::Float64) {
        if (auto d = value.as_float64()) return std::to_string(*d);
    }
    if (auto i = value.as_int64()) return std::to_string(*i);
    return "NULL";
}

[[noreturn]] void convert_fail(const core::Field& value, const std::string& type_name) {
    throw common::Exception{
        "INSERT ... SELECT: cannot convert value '" + field_display(value) + "' to column type "
            + type_name,
        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

// Convert a source value to the target column's type, throwing on any value that does not fit.
auto convert_field(const core::Field& value, const std::string& type_name) -> core::Field {
    const auto ft = value.type();

    if (is_integer_type(type_name)) {
        if (ft == core::FieldType::String) {
            const auto text = trim_ws(*value.as_string());
            if (text.empty()) convert_fail(value, type_name);
            try {
                size_t pos = 0;
                const long long parsed = std::stoll(text, &pos);
                if (pos != text.size()) convert_fail(value, type_name);
                return core::Field(static_cast<int64_t>(parsed));
            } catch (const std::exception&) {
                convert_fail(value, type_name);
            }
        }
        if (ft == core::FieldType::Float32 || ft == core::FieldType::Float64) {
            const double d = value.as_float64().value();
            if (std::isnan(d) || std::isinf(d) || std::floor(d) != d) convert_fail(value, type_name);
            return core::Field(static_cast<int64_t>(d));
        }
        if (auto i = value.as_int64()) return core::Field(*i);
        convert_fail(value, type_name);
    }

    if (type_name == "Float64" || type_name == "Float32") {
        if (ft == core::FieldType::String) {
            const auto text = trim_ws(*value.as_string());
            if (text.empty()) convert_fail(value, type_name);
            try {
                size_t pos = 0;
                const double parsed = std::stod(text, &pos);
                if (pos != text.size()) convert_fail(value, type_name);
                return core::Field(parsed);
            } catch (const std::exception&) {
                convert_fail(value, type_name);
            }
        }
        if (auto d = value.as_float64()) return core::Field(*d);
        convert_fail(value, type_name);
    }

    if (type_name == "UInt8" || type_name == "Bool") {
        if (ft == core::FieldType::String) {
            const auto text = trim_ws(*value.as_string());
            if (text == "1" || text == "true" || text == "TRUE") return core::Field(true);
            if (text == "0" || text == "false" || text == "FALSE") return core::Field(false);
            convert_fail(value, type_name);
        }
        if (auto b = value.as_bool()) return core::Field(*b);
        convert_fail(value, type_name);
    }

    // String and any other textual column type: accept a string directly, render numbers as text.
    if (auto s = value.as_string()) return core::Field(*s);
    if (ft == core::FieldType::Float32 || ft == core::FieldType::Float64) {
        if (auto d = value.as_float64()) return core::Field(std::to_string(*d));
    }
    if (auto i = value.as_int64()) return core::Field(std::to_string(*i));
    convert_fail(value, type_name);
}

} // namespace

auto InterpreterInsertQuery::build_insert_block(
    const storages::IStorage& storage,
    const std::vector<std::string>& columns,
    const std::vector<std::vector<std::string>>& values) -> core::Block {
    auto storage_columns = storage.columns();
    auto col_types       = storage.column_types();

    std::vector<std::string> target_columns = columns;
    if (target_columns.empty()) {
        target_columns = storage_columns;
    }

    core::Block block;
    for (size_t col_idx = 0; col_idx < target_columns.size(); ++col_idx) {
        const auto& col_name = target_columns[col_idx];
        auto type_it         = col_types.find(col_name);
        if (type_it == col_types.end()) {
            throw common::Exception{
                "Unknown column: " + col_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
        }

        auto* raw_col = type_it->second->create_column();
        auto col      = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw_col),
            [](void* p) { delete static_cast<core::IColumn*>(p); });

        for (const auto& row : values) {
            if (col_idx < row.size()) {
                insert_typed_value(*col, type_it->second, row[col_idx]);
            } else {
                col->insert_default();
            }
        }

        block.add_column(col_name, col);
    }

    return block;
}

auto InterpreterInsertQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    const auto& insert = query.insert;

    // INSERT INTO t [(cols)] SELECT ...  — load the result of a query (which may itself read from
    // a CONNECTOR), converting each value to the destination column type. A single value that does
    // not fit aborts the whole load: the block is built in full before anything is written.
    if (insert.select) {
        auto storage = ddl_utils::resolve_storage(context, insert.table);
        if (!storage) {
            throw common::Exception{
                "Unknown table: " + insert.table,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        const auto storage_columns = storage->columns();
        const auto col_types       = storage->column_types();

        std::vector<std::string> target =
            insert.columns.empty() ? storage_columns : insert.columns;
        for (const auto& name : target) {
            if (col_types.find(name) == col_types.end()) {
                throw common::Exception{
                    "Unknown column: " + name,
                    static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
            }
        }

        auto source = InterpreterSelectQuery::execute(context, *insert.select);
        if (source.column_count() != target.size()) {
            throw common::Exception{
                "INSERT ... SELECT column count mismatch: target has "
                    + std::to_string(target.size()) + " columns, SELECT returns "
                    + std::to_string(source.column_count()),
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        const size_t row_count = source.row_count();

        std::unordered_map<std::string, size_t> target_index;
        for (size_t i = 0; i < target.size(); ++i) {
            target_index[target[i]] = i;
        }

        core::Block block;
        for (const auto& name : storage_columns) {
            const auto& type = col_types.at(name);
            auto* raw_col    = type->create_column();
            auto col         = std::shared_ptr<core::IColumn>(
                static_cast<core::IColumn*>(raw_col),
                [](void* p) { delete static_cast<core::IColumn*>(p); });

            auto it = target_index.find(name);
            if (it != target_index.end()) {
                for (size_t r = 0; r < row_count; ++r) {
                    col->insert(convert_field(source.get_row_value(it->second, r), type->name()));
                }
            } else {
                for (size_t r = 0; r < row_count; ++r) {
                    col->insert_default();
                }
            }
            block.add_column(name, col);
        }

        storage->write(block);
        pipelines::PipelineEventBus::instance().table_inserted(
            ddl_utils::qualify_table_name(context, insert.table));
        return ddl_utils::make_ok_block();
    }

    if (insert.values.empty()) {
        throw common::Exception{
            "INSERT requires VALUES clause",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    auto& stream_mgr = streaming::StreamManager::instance();
    if (stream_mgr.has_stream(insert.table)) {
        for (const auto& row : insert.values) {
            if (row.empty()) {
                throw common::Exception{
                    "INSERT INTO stream requires at least one value",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            stream_mgr.append_to_stream(insert.table, strip_quotes(row.front()));
        }
        pipelines::PipelineEventBus::instance().stream_appended(insert.table);
        return ddl_utils::make_ok_block();
    }

    auto storage = ddl_utils::resolve_storage(context, insert.table);
    if (!storage) {
        throw common::Exception{
            "Unknown table: " + insert.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto block = build_insert_block(*storage, insert.columns, insert.values);
    storage->write(block);
    pipelines::PipelineEventBus::instance().table_inserted(
        ddl_utils::qualify_table_name(context, insert.table));

    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
