// src/Core/column.h — IColumn interface: column-oriented data abstraction
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mnesso::core {

class IColumn;
using ColumnPtr = std::shared_ptr<IColumn>;
using ColumnMutablePtr = std::shared_ptr<IColumn>;

// ── IColumn — abstract column interface ──
// All concrete columns (Vector, String, Array, etc.) implement this.
// Columns are always stored in a row-oriented batch (a Block).
class IColumn {
public:
    virtual ~IColumn() = default;

    // Row count
    [[nodiscard]] virtual auto size() const -> size_t = 0;

    // Mutability
    virtual auto mutability() const -> bool = 0;

    // Insert a value — returns the row index of the inserted value
    virtual auto insert(const Field& value) -> size_t = 0;
    virtual auto insert_default()                       -> size_t = 0;

    // Bulk insert from another column
    virtual auto insert_many_default(size_t count) -> size_t = 0;
    virtual auto insert_range(IColumn& source,
                              size_t start,
                              size_t finish) -> size_t = 0;

    // Get value at row index
    [[nodiscard]] virtual Field get(size_t row_idx) const = 0;

    // Set value at row index (only for mutable columns)
    virtual void set(size_t row_idx, const Field& value) = 0;

    // Swap two rows
    virtual void swap_rows(size_t a, size_t b) = 0;

    // Clone / copy
    [[nodiscard]] virtual auto clone() const -> ColumnPtr = 0;
    [[nodiscard]] virtual auto clone_empty() -> ColumnPtr = 0;

    // Permutation — reorder rows (used in sorting)
    virtual void permute(std::vector<size_t> indices) = 0;

    // Filter — keep only rows matching predicate
    virtual void filter(std::vector<bool> mask) = 0;

    // Compression helper — pack into contiguous memory region
    virtual auto packed_size() const -> size_t = 0;
    virtual void pack(std::vector<uint8_t>& out) const = 0;
    virtual void unpack(std::span<const uint8_t> in) = 0;

    // Type info
    [[nodiscard]] virtual auto type_name() const -> std::string = 0;

    // Compression
    virtual auto compress(std::vector<uint8_t>& buffer) -> size_t = 0;
    virtual void  decompress(std::span<const uint8_t> compressed) = 0;
};

} // namespace mnesso::core
