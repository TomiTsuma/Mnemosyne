// src/Loggers/target_console.h — Console log target
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "logger.h"
#include <string>

namespace mnesso::loggers {

// ── ConsoleTarget — logs to stdout/stderr ──
class ConsoleTarget final : public LogTarget {
public:
    static auto create() -> std::shared_ptr<ConsoleTarget>;

    [[nodiscard]] auto name()     const -> std::string override;
    auto emit(const LogEntry& entry) -> bool override;

    void set_level(LogLevel level);
    [[nodiscard]] auto level() const -> LogLevel;

    // Use stderr for ERROR+
    void set_error_to_stderr(bool value = true) { error_to_stderr_ = value; }
    [[nodiscard]] auto error_to_stderr() const -> bool { return error_to_stderr_; }

private:
    ConsoleTarget();
    LogLevel level_  = LogLevel::TRACE;
    bool     error_to_stderr_ = true;
};

} // namespace mnesso::loggers
