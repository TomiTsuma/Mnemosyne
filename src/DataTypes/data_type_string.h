// src/DataTypes/data_type_string.h — String type implementations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "data_type.h"
#include <string>
#include <string_view>
#include <optional>
#include <span>

namespace mnesso::datatypes {

// ── DataTypeString — variable-length UTF-8 string ──
class DataTypeString final : public IDataType {
public:
    [[nodiscard]] TypeId   id()    const override;
    [[nodiscard]] auto     name()  const -> std::string override;
    [[nodiscard]] auto     data_size() const -> size_t override;
    [[nodiscard]] auto     create_column() const -> void* override;
    void serialize(std::span<const uint8_t> data,
                   std::vector<uint8_t>& out) const override;
    auto  deserialize(std::span<const uint8_t> in) -> void* override;
    void to_string(std::string& out, std::span<const uint8_t> data) const override;
    auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> override;

    // String-specific
    static auto make() -> DataTypePtr;
};

// ── DataTypeFixedString — fixed-length byte string ──
class DataTypeFixedString final : public IDataType {
public:
    static auto make(size_t n) -> DataTypePtr;

    [[nodiscard]] TypeId   id()    const override;
    [[nodiscard]] auto     name()  const -> std::string override;
    [[nodiscard]] auto     data_size() const -> size_t override;
    [[nodiscard]] auto     create_column() const -> void* override;
    void serialize(std::span<const uint8_t> data,
                   std::vector<uint8_t>& out) const override;
    auto  deserialize(std::span<const uint8_t> in) -> void* override;
    void to_string(std::string& out, std::span<const uint8_t> data) const override;
    auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> override;

    [[nodiscard]] size_t fixed_size() const;

private:
    explicit DataTypeFixedString(size_t fixed_size);
    size_t fixed_size_;
};

} // namespace mnesso::datatypes
