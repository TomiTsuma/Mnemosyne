// src/Loggers/logger.h — Logging infrastructure
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <mutex>

namespace mnemo::loggers {

// ── Log levels ──
enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERR   = 4,
    FATAL = 5,
};

// ── Log entry — single log record ──
struct LogEntry {
    LogLevel   level;
    std::string message;
    std::string component;
    std::chrono::system_clock::time_point timestamp;
    std::string thread_id;
};

// ── LogTarget — output destination ──
class LogTarget {
public:
    virtual ~LogTarget() = default;
    virtual auto emit(const LogEntry& entry) -> bool = 0;
    [[nodiscard]] virtual auto name() const -> std::string = 0;
};

// ── Logger — manages log targets and log level ──
class Logger {
public:
    // Get singleton
    static auto get_instance() -> Logger&;

    // Log methods
    void trace(std::string_view msg, std::string_view component = "");
    void debug(std::string_view msg, std::string_view component = "");
    void info(std::string_view msg, std::string_view component = "");
    void warn(std::string_view msg, std::string_view component = "");
    void error(std::string_view msg, std::string_view component = "");
    void fatal(std::string_view msg, std::string_view component = "");

    // Log level control
    void set_level(LogLevel level);
    [[nodiscard]] auto level() const -> LogLevel;

    // Target management
    void add_target(std::shared_ptr<LogTarget> target);
    void remove_target(std::string_view name);
    [[nodiscard]] auto targets() const -> std::vector<std::string>;
    void clear_targets();

    // Flush all pending logs
    void flush();

private:
    Logger();
    LogLevel     level_ = LogLevel::INFO;
    std::vector<std::shared_ptr<LogTarget>> targets_;
    mutable std::mutex mutex_;

    void do_log(LogLevel level, std::string_view msg, std::string_view component);
};

} // namespace mnemo::loggers
