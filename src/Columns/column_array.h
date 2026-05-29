// src/Columns/column_array.h — ColumnArray: nested array column
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_column.h"
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include "core/field.h"

namespace mnesso::columns {

// ── ColumnArray — array of arrays, each sub-array is a pointer to column data ──
// Each element is itself a column (e.g. Array(UInt8) is Array of bytes).
class ColumnArray final : public IColumn {
public:
    static auto create() -> std::shared_ptr<IColumn>;

    [[nodiscard]] auto size()    const -> size_t override;
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnArray"; }

    // Value access
    [[nodiscard]] auto get(size_t row_idx) const -> std::optional<std::vector<Field>>;

    // Insertion
    auto insert(const Field& value) -> size_t override;
    auto insert_default() -> size_t override;

    // Sub-array access
    [[nodiscard]] auto get_sub_column(size_t row_idx) const -> ColumnPtr;
    [[nodiscard]] auto sub_column_offsets() const -> const std::vector<uint32_t>&;

    // Compression
    auto packed_size() const -> size_t override;
    void pack(std::vector<uint8_t>& out) const override;
    void unpack(std::span<const uint8_t> in) override;

    // Swap / filter
    void swap_rows(size_t a, size_t b) override;
    void filter(std::vector<bool> mask) override;

    // Clone
    auto clone()    const -> ColumnPtr override;
    auto clone_empty() -> ColumnPtr override;

private:
    std::vector<ColumnPtr> sub_columns_;
    std::vector<uint32_t>  offsets_;
    size_t                 size_ = 0;
};

} // namespace mnesso::columns
