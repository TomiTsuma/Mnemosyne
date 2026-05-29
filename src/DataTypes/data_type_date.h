// src/DataTypes/data_type_date.h — Date/DateTime type implementations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "data_type.h"
#include <string>
#include <string_view>
#include <optional>
#include <span>

namespace mnesso::datatypes {

// ── DataTypeDate — calendar date (days since 1970-01-01) ──
class DataTypeDate final : public IDataType {
public:
    static auto make() -> DataTypePtr;

    [[nodiscard]] TypeId   id()    const override;
    [[nodiscard]] auto     name()  const -> std::string override;
    [[nodiscard]] auto     data_size() const -> size_t override;
    [[nodiscard]] auto     create_column() const -> void* override;
    void serialize(std::span<const uint8_t> data,
                   std::vector<uint8_t>& out) const override;
    auto  deserialize(std::span<const uint8_t> in) -> void* override;
    void to_string(std::string& out, std::span<const uint8_t> data) const override;
    auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> override;

    // Date arithmetic
    [[nodiscard]] auto to_days()    const -> int32_t;
    [[nodiscard]] auto to_date_str() const -> std::string;

    static auto from_date_str(std::string_view s) -> std::optional<int32_t>;
    static auto from_ymd(int y, int m, int d) -> std::optional<int32_t>;

private:
    DataTypeDate() = default;
};

// ── DataTypeDateTime — timestamp (seconds since Unix epoch) ──
class DataTypeDateTime final : public IDataType {
public:
    static auto make() -> DataTypePtr;

    [[nodiscard]] TypeId   id()    const override;
    [[nodiscard]] auto     name()  const -> std::string override;
    [[nodiscard]] auto     data_size() const -> size_t override;
    [[nodiscard]] auto     create_column() const -> void* override;
    void serialize(std::span<const uint8_t> data,
                   std::vector<uint8_t>& out) const override;
    auto  deserialize(std::span<const uint8_t> in) -> void* override;
    void to_string(std::string& out, std::span<const uint8_t> data) const override;
    auto  from_string(std::string_view text) -> std::optional<std::vector<uint8_t>> override;

    [[nodiscard]] auto to_timestamp() const -> int64_t;
    static auto from_timestamp(int64_t ts) -> int64_t;

private:
    DataTypeDateTime() = default;
};

} // namespace mnesso::datatypes
