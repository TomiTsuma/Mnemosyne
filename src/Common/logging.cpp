// src/Common/logging.cpp — Logging infrastructure implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "logging.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <mutex>

namespace mnemo::common {

namespace {
    std::atomic<LogLevel> global_level{LogLevel::INFO};
    std::mutex            log_mutex;

    std::string level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKN ";
        }
    }

    std::string current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
    #ifdef _WIN32
        localtime_s(&tm_buf, &time);
    #else
        localtime_r(&time, &tm_buf);
    #endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
} // namespace

void log_impl(LogLevel level, std::string_view msg, const char* file, int line) {
    if (level < get_log_level()) return;

    std::ostringstream out;
    out << "[" << current_timestamp() << "] "
        << "[" << level_to_string(level) << "] ";

    if (file) {
        out << file << ":" << line << " - ";
    }
    out << msg << "\n";

    std::lock_guard lock{log_mutex};
    if (level >= LogLevel::ERROR) {
        std::cerr << out.str();
    } else {
        std::cout << out.str();
    }
}

void set_log_level(LogLevel level) { global_level.store(level); }
LogLevel get_log_level() { return global_level.load(); }

} // namespace mnemo::common
