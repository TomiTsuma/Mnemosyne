// src/Common/span_compat.h — ensures std::span is available (C++20 <span> or polyfill)
#pragma once

#include <cstddef>
#include <array>
#include <type_traits>

#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#  define MN_CXX20_OR_GREATER 1
#endif

#ifndef MN_CXX20_OR_GREATER
#  error "Mnemosyne requires C++20 or newer. Set CMAKE_CXX_STANDARD to 20+ (MSVC: /std:c++20)."
#endif

// ── Standard library <span> when the feature is actually provided ──
#if defined(__cpp_lib_span) && __cpp_lib_span >= 201803L
#  include <span>
#  define MN_HAS_STD_SPAN 1
#elif defined(__has_include)
#  if __has_include(<span>)
#    include <span>
#    if defined(__cpp_lib_span) && __cpp_lib_span >= 201803L
#      define MN_HAS_STD_SPAN 1
#    endif
#  endif
#endif

#if !defined(MN_HAS_STD_SPAN) && defined(__has_include) && __has_include(<experimental/span>)
#  include <experimental/span>
#  define MN_HAS_STD_EXPERIMENTAL_SPAN 1
#endif

#if defined(MN_HAS_STD_EXPERIMENTAL_SPAN) && !defined(MN_HAS_STD_SPAN)
namespace std { using std::experimental::span; }
#  define MN_HAS_STD_SPAN 1
#endif

// ── Minimal polyfill (e.g. MSVC with <span> stub before C++20 mode is active) ──
#if !defined(MN_HAS_STD_SPAN)

namespace mnesso::compat {

inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

template<typename T, std::size_t Extent = dynamic_extent>
class span {
public:
    using element_type     = T;
    using value_type       = std::remove_cv_t<T>;
    using size_type        = std::size_t;
    using difference_type  = std::ptrdiff_t;
    using pointer          = T*;
    using const_pointer    = const T*;
    using reference        = T&;
    using const_reference  = const T&;
    using iterator         = pointer;
    using const_iterator   = const_pointer;

    static constexpr size_type extent = Extent;

    constexpr span() noexcept : data_(nullptr), size_(0) {}

    constexpr span(pointer ptr, size_type count) noexcept
        : data_(ptr), size_(count) {}

    template<std::size_t N>
    constexpr span(element_type (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    template<std::size_t N>
    constexpr span(std::array<value_type, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    template<std::size_t N>
    constexpr span(const std::array<value_type, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    template<typename U, std::size_t N,
             typename = std::enable_if_t<(Extent == dynamic_extent || Extent == N)>>
    constexpr span(const span<U, N>& other) noexcept
        : data_(other.data()), size_(other.size()) {}

    [[nodiscard]] constexpr pointer    data() const noexcept { return data_; }
    [[nodiscard]] constexpr size_type  size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool       empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr reference  operator[](size_type idx) const { return data_[idx]; }
    [[nodiscard]] constexpr pointer    begin() const noexcept { return data_; }
    [[nodiscard]] constexpr pointer    end() const noexcept { return data_ + size_; }

private:
    pointer   data_;
    size_type size_;
};

} // namespace mnesso::compat

namespace std {
template<typename T, std::size_t Extent = mnesso::compat::dynamic_extent>
using span = mnesso::compat::span<T, Extent>;
} // namespace std

#  define MN_HAS_STD_SPAN 1
#endif
