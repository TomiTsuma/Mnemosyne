// src/Columns/column_vector.h — ColumnVector<T>: fixed-size typed column
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_column.h"
#include <span>
#include <vector>
#include <memory>
#include <concepts>
#include "core/field.h"

namespace mnesso::columns {

// ── ColumnVector<T> — contiguous memory array of fixed-size elements ──
// This is the most common column type — every numeric type, date, etc.
// is stored in a ColumnVector.
template<std::unsigned_integral T>
class ColumnVector {
public:
    using value_type = T;

    // ── Column lifecycle ──
    [[nodiscard]] auto size()    const -> size_t override;
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnVector<" + type_name<T>() + ">"; }

    // ── Value access ──
    [[nodiscard]] T        get(size_t row_idx) const;
    [[nodiscard]] auto     get_span() const -> std::span<const T> { return {data_.data(), size_}; }
    [[nodiscard]] auto     get_mutable_span() -> std::span<T> { return {data_.data(), size_}; }

    // ── Insertion ──
    auto insert(const Field& value) -> size_t override;
    auto insert_default() -> size_t override;

    // ── Compression ──
    auto packed_size() const -> size_t override;
    void pack(std::vector<uint8_t>& out) const override;
    void unpack(std::span<const uint8_t> in) override;

    // ── SIMD helpers — vectorized arithmetic ──
    template<typename Op>
    void apply_op(Series<T> out, Series<T> other, Op op) {
        for (size_t i = 0; i < size_; ++i) {
            out[i] = op(data_[i], other[i]);
        }
    }

    // ── Factory ──
    template<typename U>
    static auto create() -> std::shared_ptr<IColumn> {
        return std::make_shared<ColumnVector<U>>();
    }

private:
    static std::string type_name() {
        if constexpr (std::same_as<T, uint8_t>)  return "UInt8";
        if constexpr (std::same_as<T, uint16_t>) return "UInt16";
        if constexpr (std::same_as<T, uint32_t>) return "UInt32";
        if constexpr (std::same_as<T, uint64_t>) return "UInt64";
        if constexpr (std::same_as<T, int8_t>)   return "Int8";
        if constexpr (std::same_as<T, int16_t>)  return "Int16";
        if constexpr (std::same_as<T, int32_t>)  return "Int32";
        if constexpr (std::same_as<T, int64_t>)  return "Int64";
        return "Unknown";
    }

    std::vector<T> data_;
    size_t size_ = 0;
};

} // namespace mnesso::columns
