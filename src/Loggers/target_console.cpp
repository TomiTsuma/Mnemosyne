// src/Loggers/target_console.cpp — ConsoleTarget implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Loggers/target_console.h"
#include <iostream>
#include <iomanip>

namespace mnemo::loggers {

// ── ConsoleTarget ──

auto ConsoleTarget::create() -> std::shared_ptr<ConsoleTarget> {
    return std::shared_ptr<ConsoleTarget>(new ConsoleTarget());
}

ConsoleTarget::ConsoleTarget() = default;

auto ConsoleTarget::name() const -> std::string {
    return "Console";
}

auto ConsoleTarget::emit(const LogEntry& entry) -> bool {
    const char* level_str = nullptr;
    switch (entry.level) {
        case LogLevel::TRACE:   level_str = "TRACE";  break;
        case LogLevel::DEBUG:   level_str = "DEBUG";  break;
        case LogLevel::INFO:    level_str = "INFO";   break;
        case LogLevel::WARN:    level_str = "WARN";   break;
        case LogLevel::ERR:     level_str = "ERROR";  break;
        case LogLevel::FATAL:   level_str = "FATAL";  break;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time_t);
#else
    localtime_r(&time_t, &buf);
#endif

    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &buf);

    std::ostream& out = (error_to_stderr_ && entry.level >= LogLevel::ERR)
                            ? std::cerr
                            : std::cout;

    out << time_buffer << " [" << level_str << "] "
        << entry.message << "\n";

    return true;
}

void ConsoleTarget::set_level(LogLevel level) {
    level_ = level;
}

auto ConsoleTarget::level() const -> LogLevel {
    return level_;
}

} // namespace mnemo::loggers
