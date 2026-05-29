// src/Columns/i_column.h — IColumn interface (re-export with column-specific additions)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "core/field.h"

namespace mnesso::columns {

// Re-export from core
using core::Field;
using core::IColumn;

// ── Column metadata extension ──
enum class ColumnKind : uint8_t {
    Vector,  // Fixed-size typed array (UInt8, Int64, etc.)
    String,  // Variable-length strings with offset table
    Array,   // Nested arrays
    Map,     // Key-value pairs (TODO)
    Nested,  // Heterogeneous nested columns (TODO)
    LowCardinality, // Dictionary-encoded (TODO)
};

// ── ColumnFlags — serialization hints ──
enum class ColumnFlags : uint8_t {
    None    = 0,
    Nullable = 1,
    Compressed = 2,
    Sorted   = 4,
    Sparse   = 8,
};

// ── ColumnMetadata — structural info about a column ──
struct ColumnMetadata {
    ColumnKind  kind;
    size_t      element_size;
    size_t      data_capacity;
    size_t      null_count;
    ColumnFlags flags;
    std::string type_name;
};

} // namespace mnesso::columns
