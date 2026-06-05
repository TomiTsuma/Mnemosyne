// src/DataTypes/data_type.h — IDataType: abstract type system for Mnemosyne columns
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include "Common/span_compat.h"
#include <cstdint>
#include <vector>
#include <optional>

namespace mnemo::datatypes {

class IDataType;
using DataTypePtr = std::shared_ptr<IDataType>;

// ── TypeId — enum for all primitive types ──
enum class TypeId : uint8_t {
    UInt8, UInt16, UInt32, UInt64,
    Int8,  Int16, Int32,  Int64,
    Float32, Float64,
    String, FixedString,
    Date, DateTime,
    Array,
    UUID,
    Bool,
    Decimal,
    Unknown,
};

// ── IDataType — abstract base for all column value types ──
// Each type knows its size, serialization format, and how to create
// a matching column implementation.
class IDataType {
public:
    virtual ~IDataType() = default;

    // Type identification
    [[nodiscard]] virtual TypeId id()           const = 0;
    [[nodiscard]] virtual auto   name()         const -> std::string = 0;
    [[nodiscard]] virtual auto   data_size()    const -> size_t      = 0; // bytes per element

    // Column creation
    [[nodiscard]] virtual auto create_column() const -> void* = 0;

    // Serialization
    virtual void serialize(std::span<const uint8_t> data,
                           std::vector<uint8_t>& out) const = 0;
    virtual auto  deserialize(std::span<const uint8_t> in) -> void* = 0;

    // Text representation
    virtual void to_string(std::string& out, std::span<const uint8_t> data) const = 0;
    virtual auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> = 0;

    // Comparability
    [[nodiscard]] virtual bool is_comparable() const { return true; }
    [[nodiscard]] virtual bool is_orderable()  const { return true; }
    [[nodiscard]] virtual auto sizeof_impl() const -> size_t { return data_size(); }
};

// ── Type comparison helper ──
bool types_are_compatible(TypeId a, TypeId b);
bool types_are_equivalent(const DataTypePtr& a, const DataTypePtr& b);

// ── Type name resolution ──
TypeId resolve_type_id(std::string_view name);
std::string type_id_to_string(TypeId id);

} // namespace mnemo::datatypes
