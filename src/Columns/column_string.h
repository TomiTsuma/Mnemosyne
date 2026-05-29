// src/Columns/column_string.h — ColumnString: variable-length string column
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_column.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <span>
#include "core/field.h"

namespace mnesso::columns {

// ── ColumnString — variable-length UTF-8 strings with offset table ──
// Strings are stored in a single contiguous buffer. Each row's offset
// is stored in a parallel uint32_t array.
class ColumnString final : public IColumn {
public:
    [[nodiscard]] auto size()    const -> size_t override;
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnString"; }

    // Value access
    [[nodiscard]] auto get(size_t row_idx) const -> std::string_view;
    [[nodiscard]] auto get_span(size_t row_idx) const -> std::span<const char>;

    // Insertion
    auto insert(const Field& value) -> size_t override;
    auto insert_default() -> size_t override;

    // Bulk operations
    auto insert_str(std::string_view s) -> size_t;

    // Compression
    auto packed_size() const -> size_t override;
    void pack(std::vector<uint8_t>& out) const override;
    void unpack(std::span<const uint8_t> in) override;

    // Swap rows
    void swap_rows(size_t a, size_t b) override;
    void filter(std::vector<bool> mask) override;

    // Clone
    auto clone()    const -> ColumnPtr override;
    auto clone_empty() -> ColumnPtr override;

private:
    // Internal representation
    struct StringSlot {
        uint32_t offset = 0;
        uint32_t length = 0;
    };

    std::vector<char> data_;     // contiguous string data
    std::vector<StringSlot> slots_;
    size_t size_ = 0;
};

} // namespace mnesso::columns
