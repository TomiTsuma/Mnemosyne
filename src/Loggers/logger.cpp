// src/Loggers/logger.cpp — Logging layer implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Loggers/logger.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

namespace mnemo::loggers {

// ── Logger ──

Logger::Logger() = default;

static const char* format_log_level(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARN:    return "WARN";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
    }
    return "UNKNOWN";
}

auto Logger::get_instance() -> Logger& {
    static Logger instance;
    return instance;
}

void Logger::trace(std::string_view msg, std::string_view component) {
    do_log(LogLevel::TRACE, msg, component);
}

void Logger::debug(std::string_view msg, std::string_view component) {
    do_log(LogLevel::DEBUG, msg, component);
}

void Logger::info(std::string_view msg, std::string_view component) {
    do_log(LogLevel::INFO, msg, component);
}

void Logger::warn(std::string_view msg, std::string_view component) {
    do_log(LogLevel::WARN, msg, component);
}

void Logger::error(std::string_view msg, std::string_view component) {
    do_log(LogLevel::ERR, msg, component);
}

void Logger::fatal(std::string_view msg, std::string_view component) {
    do_log(LogLevel::FATAL, msg, component);
}

void Logger::set_level(LogLevel level) {
    std::lock_guard lock(mutex_);
    level_ = level;
}

auto Logger::level() const -> LogLevel {
    return level_;
}

void Logger::add_target(std::shared_ptr<LogTarget> target) {
    std::lock_guard lock(mutex_);
    targets_.push_back(std::move(target));
}

void Logger::remove_target(std::string_view name) {
    std::lock_guard lock(mutex_);
    targets_.erase(
        std::remove_if(targets_.begin(), targets_.end(), [&](const auto& target) {
            return target->name() == name;
        }),
        targets_.end());
}

auto Logger::targets() const -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(targets_.size());
    for (const auto& target : targets_) {
        names.push_back(target->name());
    }
    return names;
}

void Logger::clear_targets() {
    std::lock_guard lock(mutex_);
    targets_.clear();
}

void Logger::flush() {
    std::lock_guard lock(mutex_);
    for (auto& target : targets_) {
        target->emit(LogEntry{});
    }
}

void Logger::do_log(LogLevel level, std::string_view msg, std::string_view component) {
    if (level < level_) return;

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

    LogEntry entry{
        level,
        std::string{msg},
        std::string{component},
        now,
        std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()))
    };

    std::lock_guard lock(mutex_);
    for (const auto& target : targets_) {
        target->emit(entry);
    }

    std::cout << time_buffer << " [" << format_log_level(level) << "] "
              << entry.message << "\n";
}

} // namespace mnemo::loggers
