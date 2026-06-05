// src/Columns/column_vector.h — ColumnVector<T>: fixed-size typed column
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_column.h"
#include "Core/column.h"
#include "Core/field.h"
#include "Core/series.h"
#include "Common/span_compat.h"
#include "DataTypes/data_type_number.h"
#include <vector>
#include <memory>
#include <concepts>
#include <cstring>

namespace mnemo::columns {

// ── ColumnVector<T> — contiguous memory array of fixed-size elements ──
// This is the most common column type — every numeric type, date, etc.
// is stored in a ColumnVector.
template<typename T>
class ColumnVector : public IColumn {
public:
    using value_type = T;

    // ── Column lifecycle ──
    auto clear() -> size_t override {
        data_.clear();
        size_ = 0;
        return 0;
    }
    [[nodiscard]] auto size()    const -> size_t override { return size_; }
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnVector<" + type_name_impl<T>() + ">"; }

    // ── Value access ──
    [[nodiscard]] core::Field get(size_t row_idx) const override {
        if (row_idx >= size_) return core::Field{};
        return core::Field(data_[row_idx]);
    }
    [[nodiscard]] core::Field get_at(size_t row_idx) const override {
        return get(row_idx);
    }
    [[nodiscard]] auto get_data_type() const -> datatypes::DataTypePtr override {
        if constexpr (std::same_as<T, uint8_t>)  return datatypes::DataTypeNumber::make(datatypes::TypeId::UInt8);
        if constexpr (std::same_as<T, uint16_t>) return datatypes::DataTypeNumber::make(datatypes::TypeId::UInt16);
        if constexpr (std::same_as<T, uint32_t>) return datatypes::DataTypeNumber::make(datatypes::TypeId::UInt32);
        if constexpr (std::same_as<T, uint64_t>) return datatypes::DataTypeNumber::make(datatypes::TypeId::UInt64);
        if constexpr (std::same_as<T, int8_t>)   return datatypes::DataTypeNumber::make(datatypes::TypeId::Int8);
        if constexpr (std::same_as<T, int16_t>)  return datatypes::DataTypeNumber::make(datatypes::TypeId::Int16);
        if constexpr (std::same_as<T, int32_t>)  return datatypes::DataTypeNumber::make(datatypes::TypeId::Int32);
        if constexpr (std::same_as<T, int64_t>)  return datatypes::DataTypeNumber::make(datatypes::TypeId::Int64);
        if constexpr (std::same_as<T, float>)    return datatypes::DataTypeNumber::make(datatypes::TypeId::Float32);
        if constexpr (std::same_as<T, double>)   return datatypes::DataTypeNumber::make(datatypes::TypeId::Float64);
        return nullptr;
    }
    [[nodiscard]] auto insert_at(size_t row_idx, const Field& value) -> size_t override {
        if (row_idx >= size_) {
            if (row_idx > size_) insert_many_default(row_idx - size_);
            return insert(value);
        }
        set(row_idx, value);
        return row_idx;
    }
    [[nodiscard]] auto     get_span() const -> std::span<const T> { return {data_.data(), size_}; }
    [[nodiscard]] auto     get_mutable_span() -> std::span<T> { return {data_.data(), size_}; }

    // ── Insertion ──
    auto insert(const Field& value) -> size_t override {
        if (value.type() == core::FieldType::Null) {
            data_.push_back(T{});
        } else {
            auto v = value.as_int64();
            if (v) {
                data_.push_back(static_cast<T>(*v));
            } else {
                auto fv = value.as_float64();
                if (fv) {
                    data_.push_back(static_cast<T>(*fv));
                } else {
                    data_.push_back(T{});
                }
            }
        }
        size_ = data_.size();
        return size_ - 1;
    }
    auto insert_default() -> size_t override {
        data_.push_back(T{});
        size_ = data_.size();
        return size_ - 1;
    }
    auto insert_many_default(size_t count) -> size_t override {
        data_.resize(size_ + count, T{});
        size_ = data_.size();
        return size_;
    }
    auto insert_range(IColumn& source, size_t start, size_t finish) -> size_t override {
        size_t inserted = 0;
        for (size_t i = start; i < finish && i < source.size(); ++i) {
            auto val = source.get(i);
            if (val.type() == core::FieldType::Null) {
                data_.push_back(T{});
            } else {
                auto v = val.as_int64();
                if (v) {
                    data_.push_back(static_cast<T>(*v));
                } else {
                    auto fv = val.as_float64();
                    if (fv) {
                        data_.push_back(static_cast<T>(*fv));
                    } else {
                        data_.push_back(T{});
                    }
                }
            }
            ++inserted;
        }
        size_ = data_.size();
        return inserted;
    }

    void set(size_t row_idx, const Field& value) override {
        if (row_idx < size_) {
            if (value.type() == core::FieldType::Null) {
                data_[row_idx] = T{};
            } else {
                auto v = value.as_int64();
                if (v) {
                    data_[row_idx] = static_cast<T>(*v);
                } else {
                    auto fv = value.as_float64();
                    if (fv) {
                        // Only allow float-to-int conversion if T is integral
                        if constexpr (std::is_integral_v<T>) {
                            data_[row_idx] = static_cast<T>(*fv);
                        } else if constexpr (std::is_floating_point_v<T>) {
                            data_[row_idx] = static_cast<T>(*fv);
                        }
                    }
                }
            }
        }
    }

    // ── Compression ──
    auto packed_size() const -> size_t override { return size_ * sizeof(T); }
    void pack(std::vector<uint8_t>& out) const override {
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(data_.data()),
                   reinterpret_cast<const uint8_t*>(data_.data()) + size_ * sizeof(T));
    }
    void unpack(std::span<const uint8_t> in) override {
        size_t count = in.size() / sizeof(T);
        data_.resize(count);
        std::memcpy(data_.data(), in.data(), count * sizeof(T));
        size_ = count;
    }

    // ── Row operations ──
    void swap_rows(size_t a, size_t b) override {
        if (a < size_ && b < size_) std::swap(data_[a], data_[b]);
    }
    void filter(std::vector<bool> mask) override {
        std::vector<T> new_data;
        new_data.reserve(size_);
        for (size_t i = 0; i < size_ && i < mask.size(); ++i) {
            if (mask[i]) new_data.push_back(data_[i]);
        }
        data_ = std::move(new_data);
        size_ = data_.size();
    }
    void permute(std::vector<size_t> indices) override {
        std::vector<T> new_data(size_);
        for (size_t i = 0; i < indices.size() && i < size_; ++i) {
            if (indices[i] < size_) new_data[i] = data_[indices[i]];
        }
        data_ = std::move(new_data);
    }

    // ── Clone ──
    [[nodiscard]] auto clone() const -> core::ColumnPtr override {
        auto col = std::make_shared<ColumnVector<T>>();
        col->data_ = data_;
        col->size_ = size_;
        return col;
    }
    [[nodiscard]] auto clone_empty() -> core::ColumnPtr override {
        return std::make_shared<ColumnVector<T>>();
    }

    // ── Compression helper ──
    auto compress(std::vector<uint8_t>& buffer) -> size_t override {
        pack(buffer);
        return buffer.size();
    }
    void decompress(std::span<const uint8_t> compressed) override {
        unpack(compressed);
    }

    // ── SIMD helpers — vectorized arithmetic ──
    template<typename Op>
    void apply_op(core::Series<T> out, core::Series<T> other, Op op) {
        for (size_t i = 0; i < size_; ++i) {
            out[i] = op(data_[i], other[i]);
        }
    }

    // ── Factory ──
    static auto create() -> std::shared_ptr<IColumn> {
        return std::make_shared<ColumnVector<T>>();
    }

    void resize(size_t n) {
        data_.resize(n, T{});
        size_ = n;
    }

private:
    template<typename U>
    static std::string type_name_impl() {
        if constexpr (std::same_as<U, uint8_t>)  return "UInt8";
        if constexpr (std::same_as<U, uint16_t>) return "UInt16";
        if constexpr (std::same_as<U, uint32_t>) return "UInt32";
        if constexpr (std::same_as<U, uint64_t>) return "UInt64";
        if constexpr (std::same_as<U, int8_t>)   return "Int8";
        if constexpr (std::same_as<U, int16_t>)  return "Int16";
        if constexpr (std::same_as<U, int32_t>)  return "Int32";
        if constexpr (std::same_as<U, int64_t>)  return "Int64";
        if constexpr (std::same_as<U, float>)    return "Float32";
        if constexpr (std::same_as<U, double>)   return "Float64";
        return "Unknown";
    }

    std::vector<T> data_;
    size_t size_ = 0;
};

} // namespace mnemo::columns
