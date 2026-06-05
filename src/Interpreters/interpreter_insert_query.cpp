// src/Interpreters/interpreter_insert_query.cpp — INSERT interpreter implementation

#include "Interpreters/interpreter_insert_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Common/exceptions.h"
#include <algorithm>

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
    auto storage = ddl_utils::resolve_storage(context, insert.table);

    if (!storage) {
        throw common::Exception{
            "Unknown table: " + insert.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    if (insert.values.empty()) {
        throw common::Exception{
            "INSERT requires VALUES clause",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    auto block = build_insert_block(*storage, insert.columns, insert.values);
    storage->write(block);

    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
