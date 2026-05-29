// src/Core/series.h — Series: utility for contiguous memory views
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mnesso::core {

// ── Series — a thin view over a contiguous numeric array ──
// Used in the vectorized execution engine as a temporary container
// for SIMD-friendly arithmetic operations.
template<typename T>
class Series {
public:
    using value_type    = T;
    using pointer       = T*;
    using const_pointer = const T*;
    using reference     = T&;
    using const_ref     = const T&;
    using size_type     = size_t;
    using iterator      = pointer;
    using const_iterator = const_pointer;

    constexpr Series() noexcept = default;
    constexpr Series(std::span<T> span) noexcept : data_(span.data()), size_(span.size()) {}
    constexpr Series(T* ptr, size_type sz) noexcept : data_(ptr), size_(sz) {}

    constexpr auto  data()       const noexcept -> pointer       { return data_; }
    constexpr auto  data()       const noexcept -> const_pointer { return data_; }
    constexpr auto  size()       const noexcept -> size_type     { return size_; }
    constexpr auto  begin()      const noexcept -> iterator      { return data_; }
    constexpr auto  end()        const noexcept -> iterator      { return data_ + size_; }
    constexpr auto  cbegin()     const noexcept -> const_iterator { return data_; }
    constexpr auto  cend()       const noexcept -> const_iterator { return data_ + size_; }
    constexpr auto  operator[](size_type idx) const noexcept -> const_ref { return data_[idx]; }
    constexpr auto  operator[](size_type idx) const noexcept -> reference { return data_[idx]; }
    constexpr auto  at(size_type idx) -> const_ref {
        if (idx >= size_) throw std::out_of_range{"Series::at: index out of range"};
        return data_[idx];
    }
    constexpr bool  empty()    const noexcept -> bool { return size_ == 0; }

    // Pointer arithmetic helpers
    constexpr auto subspan(size_type offset, size_type count = 0) const -> Series {
        size_type n = count ? count : size_ - offset;
        return {data_ + offset, n};
    }

    // Zero-fill
    constexpr void fill(T value) {
        for (size_type i = 0; i < size_; ++i) data_[i] = value;
    }

private:
    pointer    data_ = nullptr;
    size_type  size_ = 0;
};

// ── Convenience aliases ──
using SeriesUInt8   = Series<uint8_t>;
using SeriesUInt16  = Series<uint16_t>;
using SeriesUInt32  = Series<uint32_t>;
using SeriesUInt64  = Series<uint64_t>;
using SeriesInt8    = Series<int8_t>;
using SeriesInt16   = Series<int16_t>;
using SeriesInt32   = Series<int32_t>;
using SeriesInt64   = Series<int64_t>;
using SeriesFloat32 = Series<float>;
using SeriesFloat64 = Series<double>;

// ── Series math operations — for SIMD-style vectorized execution ──
template<typename T, typename Op>
void series_apply_op(Series<T> out, Series<T> a, Series<T> b, Op op) {
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = op(a[i], b[i]);
    }
}

template<typename T, typename Op>
void series_unary_op(Series<T> out, Series<T> in, Op op) {
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = op(in[i]);
    }
}

} // namespace mnesso::core
