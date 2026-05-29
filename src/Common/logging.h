#pragma once

#include <string>
#include <string_view>
#include <chrono>
#include <source_location>

// ── Log levels ──
enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
};

// ── Logging macros — all emit to the global log sink ──
// TODO: replace with actual async log sink implementation

#define LOG_TRACE(msg)  ::mnesso::common::log_impl(LogLevel::TRACE,   msg, __builtin_FILE(), __builtin_LINE())
#define LOG_DEBUG(msg)  ::mnesso::common::log_impl(LogLevel::DEBUG,   msg, __builtin_FILE(), __builtin_LINE())
#define LOG_INFO(msg)   ::mnesso::common::log_impl(LogLevel::INFO,    msg, __builtin_FILE(), __builtin_LINE())
#define LOG_WARN(msg)   ::mnesso::common::log_impl(LogLevel::WARN,    msg, __builtin_FILE(), __builtin_LINE())
#define LOG_ERROR(msg)  ::mnesso::common::log_impl(LogLevel::ERROR,   msg, __builtin_FILE(), __builtin_LINE())
#define LOG_FATAL(msg)  ::mnesso::common::log_impl(LogLevel::FATAL,   msg, __builtin_FILE(), __builtin_LINE())

// Fatal logs abort after writing
#define LOG_FATAL_THROW(msg) do { LOG_FATAL(msg); throw ::mnesso::common::FatalError{msg}; } while(false)

namespace mnesso::common {

// Internal logger — each macro expands to this
void log_impl(LogLevel level, std::string_view msg, const char* file, int line);

// Set the global log level (call once at startup)
void set_log_level(LogLevel level);
[[nodiscard]] LogLevel get_log_level();

// Log a formatted message — use with std::format
template<typename... Args>
void log(LogLevel level, std::string_view fmt, Args&&... args) {
    if (level < get_log_level()) return;
    // TODO: use fmt::format or std::format here
    std::string msg = /* fmt::format(fmt, args...) */ "";
    log_impl(level, msg, "", 0);
}

} // namespace mnesso::common
