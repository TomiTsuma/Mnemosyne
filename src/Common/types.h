#pragma once

#include <cstdint>
#include <vector>
#include <string_view>
#include <functional>
#include <string>
#include <chrono>

namespace mnesso::common {

// ── Numeric type aliases ──
using Int8    = std::int8_t;
using Int16   = std::int16_t;
using Int32   = std::int32_t;
using Int64   = std::int64_t;
using UInt8   = std::uint8_t;
using UInt16  = std::uint16_t;
using UInt32  = std::uint32_t;
using UInt64  = std::uint64_t;
using Float32 = float;
using Float64 = double;

// ── Block/row identifiers ──
using BlockId = UInt64;
using RowId   = UInt64;
using PartId  = UInt64;
using QueryId = std::string;
using UUID    = std::string;

// ── Common container types ──
using ColumnIndex = Int32;
using BlockSize   = size_t;

// ── Result status ──
enum class Status {
    Ok,
    Error,
    Eof,
    Incomplete,
};

// ── Byte order ──
enum class Endianness : uint8_t {
    Little = 0,
    Big    = 1,
};

// ── Thread-safe increment (atomic) ──
class Counter {
public:
    void increment() noexcept;
    void decrement() noexcept;
    [[nodiscard]] UInt64 load() const noexcept;
    void store(UInt64 value) noexcept;

private:
    UInt64 value_ = 0;
    // TODO: replace with std::atomic<UInt64>
};

// ── Scoped timer — logs duration on destruction ──
class Timer {
public:
    Timer(std::string_view label = "");
    ~Timer();

    [[nodiscard]] double elapsed_ms() const;
    void                 restart();

private:
    std::string    label_;
    std::chrono::steady_clock::time_point start_;
};

// ── Scope guard — runs cleanup on scope exit ──
template<typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F f) : func_(std::move(f)), active_(true) {}
    ~ScopeGuard() { if (active_) func_(); }
    void dismiss() { active_ = false; }
    ScopeGuard(ScopeGuard&& other) noexcept : func_(std::move(other.func_)), active_(other.active_) {
        other.active_ = false;
    }
    ScopeGuard(const ScopeGuard&) = delete;
    void operator=(const ScopeGuard&) = delete;

private:
    F     func_;
    bool  active_;
};

// ── RAII helper factory ──
template<typename F>
ScopeGuard<F> make_scope_guard(F&& f) {
    return ScopeGuard<F>(std::forward<F>(f));
}

} // namespace mnesso::common
