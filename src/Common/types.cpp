// src/Common/types.cpp — Numeric type aliases and common utilities
// Mnemosyne: A column-oriented analytical DBMS

#include "Common/types.h"
#include <chrono>
#include <format>

namespace mnesso::common {

// ── Counter ──

void Counter::increment() noexcept {
    ++value_;
}

void Counter::decrement() noexcept {
    --value_;
}

UInt64 Counter::load() const noexcept {
    return value_;
}

void Counter::store(UInt64 value) noexcept {
    value_ = value;
}

// ── Timer ──

Timer::Timer(std::string_view label)
    : label_{label}, start_{std::chrono::steady_clock::now()} {}

Timer::~Timer() {
    double elapsed = elapsed_ms();
    // In production, this would go to the logging system
    if (!label_.empty()) {
        // LOG_DEBUG(std::format("Timer[{}]: {:.3f} ms", label_, elapsed));
    }
}

double Timer::elapsed_ms() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start_).count();
}

void Timer::restart() {
    start_ = std::chrono::steady_clock::now();
}

} // namespace mnesso::common