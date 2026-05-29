// src/DataTypes/data_type_number.h — Numeric type implementations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "data_type.h"
#include <string>
#include <cstdint>
#include <optional>
#include <span>

namespace mnesso::datatypes {

// ── Concrete types ──
class DataTypeNumber final : public IDataType {
public:
    // Tagged constructor — creates the right type
    static auto make(TypeId id) -> DataTypePtr;

    // IDataType interface
    [[nodiscard]] TypeId   id()    const override;
    [[nodiscard]] auto     name()  const -> std::string override;
    [[nodiscard]] auto     data_size() const -> size_t override;
    [[nodiscard]] auto     create_column() const -> void* override;
    void serialize(std::span<const uint8_t> data,
                   std::vector<uint8_t>& out) const override;
    auto  deserialize(std::span<const uint8_t> in) -> void* override;
    void to_string(std::string& out, std::span<const uint8_t> data) const override;
    auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> override;

    // Numeric-specific operations
    auto  to_int64(std::span<const uint8_t> data) const -> int64_t;
    auto  to_uint64(std::span<const uint8_t> data) const -> uint64_t;
    auto  to_double(std::span<const uint8_t> data) const -> double;

    [[nodiscard]] auto value_size() const -> size_t;

    [[nodiscard]] bool is_integer() const;
    [[nodiscard]] bool is_floating() const;
    [[nodiscard]] bool is_signed()  const;

private:
    explicit DataTypeNumber(TypeId id, size_t size, bool signed_, bool floating);

    TypeId id_;
    size_t data_size_  = 0;
    bool   signed_     = false;
    bool   floating_   = false;
};

// ── Factory — returns the right numeric type for the given TypeId ──
class NumberTypeFactory {
public:
    static auto create(TypeId id) -> DataTypePtr;
    [[nodiscard]] static auto all_ids() -> std::vector<TypeId>;
};

} // namespace mnesso::datatypes
