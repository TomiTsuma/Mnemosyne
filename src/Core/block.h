// src/Core/block.h — Block: fundamental data unit (table of columns)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include "Core/column.h"

namespace mnemo::core {

// ── ColumnInfo — metadata for a single column in a Block ──
struct ColumnInfo {
    std::string name;
    size_t       offset;  // offset into the block's memory arena
    size_t       size;    // bytes per row for this column
    bool         nullable;
};

// ── Block — a rectangular table of columns, all same row count ──
// This is the fundamental unit of data flow in Mnemosyne's pipeline.
class Block {
public:
    Block();

    // Column access
    void add_column(std::string name, ColumnPtr column);
    void erase_column(std::string_view name);

    // Column lookup — by name or by index
    [[nodiscard]] ColumnPtr get_column(std::string_view name) const;
    [[nodiscard]] ColumnPtr get_column_by_name(std::string_view name) const;
    [[nodiscard]] ColumnPtr get_column_by_index(size_t index) const;

    // Metadata
    [[nodiscard]] auto column_count()  const -> size_t;
    [[nodiscard]] auto row_count()     const -> size_t;
    [[nodiscard]] auto has_columns()   const -> bool;
    [[nodiscard]] bool empty()         const { return row_count_ == 0; }

    // Names and types
    [[nodiscard]] auto column_names()  const -> std::vector<std::string>;
    [[nodiscard]] auto column_indices() const -> std::unordered_map<std::string, size_t>;

    // Row-level access
    Field get_row_value(size_t col_idx, size_t row_idx) const;

    // Reshape
    Block clone() const;
    void  reset();

    // Compact — free capacity
    void compact();

private:
    struct ColumnEntry {
        std::string        name;
        std::shared_ptr<IColumn> column;
        size_t             offset;
    };

    std::vector<ColumnEntry> columns_;
    size_t                   row_count_ = 0;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace mnemo::core
