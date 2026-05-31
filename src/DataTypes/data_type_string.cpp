// src/DataTypes/data_type_string.cpp — String type implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "DataTypes/data_type_string.h"
#include "Columns/i_column.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"
#include <stdexcept>
#include <cstring>

namespace mnesso::datatypes {

// ── DataTypeString ──

auto DataTypeString::make() -> DataTypePtr {
    return std::make_shared<DataTypeString>();
}

auto DataTypeString::id() const -> TypeId { return TypeId::String; }
auto DataTypeString::name() const -> std::string { return "String"; }
auto DataTypeString::data_size() const -> size_t { return 0; } // variable

auto DataTypeString::create_column() const -> void* {
    return new columns::ColumnString{};
}

void DataTypeString::serialize(std::span<const uint8_t> data,
                               std::vector<uint8_t>& out) const {
    // Prefix with length (uint32_t)
    uint32_t len = static_cast<uint32_t>(data.size());
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&len),
               reinterpret_cast<const uint8_t*>(&len) + sizeof(len));
    out.insert(out.end(), data.begin(), data.end());
}

auto DataTypeString::deserialize(std::span<const uint8_t> in) -> void* {
    if (in.size() < sizeof(uint32_t)) {
        throw common::Exception{"DataTypeString::deserialize: insufficient data",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    uint32_t len;
    std::memcpy(&len, in.data(), sizeof(len));
    if (in.size() < sizeof(uint32_t) + len) {
        throw common::Exception{"DataTypeString::deserialize: insufficient data for string",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    auto* col = new columns::ColumnString{};
    return col;
}

void DataTypeString::to_string(std::string& out,
                               std::span<const uint8_t> data) const {
    out.assign(data.begin(), data.end());
}

auto DataTypeString::from_string(std::string_view text)
    -> std::optional<std::vector<uint8_t>> {
    return std::vector<uint8_t>{text.begin(), text.end()};
}

// ── DataTypeFixedString ──

auto DataTypeFixedString::make(size_t n) -> DataTypePtr {
    return std::shared_ptr<DataTypeFixedString>(new DataTypeFixedString(n));
}

DataTypeFixedString::DataTypeFixedString(size_t fixed_size)
    : fixed_size_{fixed_size} {}

auto DataTypeFixedString::id() const -> TypeId { return TypeId::FixedString; }
auto DataTypeFixedString::name() const -> std::string {
    return "FixedString(" + std::to_string(fixed_size_) + ")";
}
auto DataTypeFixedString::data_size() const -> size_t { return fixed_size_; }
auto DataTypeFixedString::fixed_size() const -> size_t { return fixed_size_; }

auto DataTypeFixedString::create_column() const -> void* {
    return new columns::ColumnString{}; // Uses ColumnString internally
}

void DataTypeFixedString::serialize(std::span<const uint8_t> data,
                                    std::vector<uint8_t>& out) const {
    if (data.size() != fixed_size_) {
        throw common::Exception{"DataTypeFixedString::serialize: size mismatch",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    out.insert(out.end(), data.begin(), data.end());
}

auto DataTypeFixedString::deserialize(std::span<const uint8_t> in) -> void* {
    if (in.size() != fixed_size_) {
        throw common::Exception{"DataTypeFixedString::deserialize: size mismatch",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    auto* col = new columns::ColumnString{};
    return col;
}

void DataTypeFixedString::to_string(std::string& out,
                                    std::span<const uint8_t> data) const {
    out.assign(data.begin(), data.end());
}

auto DataTypeFixedString::from_string(std::string_view text)
    -> std::optional<std::vector<uint8_t>> {
    if (text.size() != fixed_size_) {
        return std::nullopt;
    }
    return std::vector<uint8_t>{text.begin(), text.end()};
}

} // namespace mnesso::datatypes