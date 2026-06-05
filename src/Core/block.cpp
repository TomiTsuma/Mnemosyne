// src/Core/block.cpp — Block: fundamental data unit (table of columns)
// Mnemosyne: A column-oriented analytical DBMS

#include "Core/block.h"
#include "Core/column.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <iostream>

using mnemo::common::Exception;
using mnemo::common::ErrorCode;

namespace mnemo::core {

Block::Block() = default;

void Block::add_column(std::string name, ColumnPtr column) {
    if (column_count() > 0 && column->size() != row_count_) {
        throw Exception{
            "Block::add_column: column '" + name +
            "' size " + std::to_string(column->size()) +
            " does not match block row count " + std::to_string(row_count_),
            static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
    ColumnEntry entry;
    entry.name = std::move(name);
    entry.column = std::move(column);
    entry.offset = 0;
    columns_.push_back(std::move(entry));
    index_[columns_.back().name] = columns_.size() - 1;
    if (!columns_.empty()) {
        row_count_ = columns_.back().column->size();
    }
}

void Block::erase_column(std::string_view name) {
    auto it = index_.find(std::string{name});
    if (it == index_.end()) {
        throw Exception{
            "Block::erase_column: column '" + std::string{name} + "' not found",
            static_cast<int>(ErrorCode::UNKNOWN_COLUMN)};
    }
    columns_.erase(columns_.begin() + static_cast<long>(it->second));
    index_.erase(std::string{name});
    // Rebuild index
    index_.clear();
    for (size_t i = 0; i < columns_.size(); ++i) {
        index_[columns_[i].name] = i;
    }
    if (!columns_.empty()) {
        row_count_ = columns_.back().column->size();
    } else {
        row_count_ = 0;
    }
}

ColumnPtr Block::get_column_by_name(std::string_view name) const {
    return get_column(name);
}

ColumnPtr Block::get_column(std::string_view name) const {
    auto it = index_.find(std::string{name});
    if (it == index_.end()) {
        throw Exception{
            "Block::get_column: column '" + std::string{name} + "' not found",
            static_cast<int>(ErrorCode::UNKNOWN_COLUMN)};
    }
    return columns_[it->second].column;
}

ColumnPtr Block::get_column_by_index(size_t index) const {
    if (index >= columns_.size()) {
        throw Exception{
            "Block::get_column_by_index: index " + std::to_string(index) +
            " out of range (columns: " + std::to_string(columns_.size()) + ")",
            static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
    return columns_[index].column;
}

auto Block::column_count() const -> size_t {
    return columns_.size();
}

auto Block::row_count() const -> size_t {
    return row_count_;
}

auto Block::has_columns() const -> bool {
    return !columns_.empty();
}

auto Block::column_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(columns_.size());
    for (const auto& col : columns_) {
        names.push_back(col.name);
    }
    return names;
}

auto Block::column_indices() const -> std::unordered_map<std::string, size_t> {
    return index_;
}

Field Block::get_row_value(size_t col_idx, size_t row_idx) const {
    if (col_idx >= columns_.size()) {
        throw Exception{
            "Block::get_row_value: column index " + std::to_string(col_idx) +
            " out of range",
            static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
    if (row_idx >= row_count_) {
        throw Exception{
            "Block::get_row_value: row index " + std::to_string(row_idx) +
            " out of range (rows: " + std::to_string(row_count_) + ")",
            static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
    return columns_[col_idx].column->get(row_idx);
}

Block Block::clone() const {
    Block result;
    for (const auto& col : columns_) {
        auto cloned = col.column->clone();
        result.add_column(col.name, cloned);
    }
    return result;
}

void Block::reset() {
    columns_.clear();
    index_.clear();
    row_count_ = 0;
}

void Block::compact() {
    for (auto& entry : columns_) {
        auto cloned = entry.column->clone();
        entry.column = cloned;
    }
}

} // namespace mnemo::core