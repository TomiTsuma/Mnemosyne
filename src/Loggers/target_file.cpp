// src/Loggers/target_file.cpp — FileTarget implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Loggers/target_file.h"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace mnesso::loggers {

// ── FileTarget ──

auto FileTarget::create(std::string path) -> std::shared_ptr<FileTarget> {
    auto target = std::shared_ptr<FileTarget>(new FileTarget());
    target->path_ = std::move(path);
    target->file_ = std::fopen(target->path_.c_str(), "a");
    if (!target->file_) {
        throw std::runtime_error("Failed to open log file: " + target->path_);
    }
    return target;
}

FileTarget::FileTarget() = default;

auto FileTarget::name() const -> std::string {
    return path_;
}

auto FileTarget::emit(const LogEntry& entry) -> bool {
    if (!file_) {
        return false;
    }

    const char* level_str = nullptr;
    switch (entry.level) {
        case LogLevel::TRACE: level_str = "TRACE"; break;
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO:  level_str = "INFO";  break;
        case LogLevel::WARN:  level_str = "WARN";  break;
        case LogLevel::ERR:   level_str = "ERROR"; break;
        case LogLevel::FATAL: level_str = "FATAL"; break;
    }

    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time_t);
#else
    localtime_r(&time_t, &buf);
#endif

    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &buf);

    std::ostringstream line;
    line << time_buffer << " [" << level_str << "] "
         << entry.message << "\n";

    auto text = line.str();
    auto written = std::fwrite(text.data(), 1, text.size(), file_);
    std::fflush(file_);
    return written == text.size();
}

void FileTarget::set_level(LogLevel level) {
    level_ = level;
}

auto FileTarget::level() const -> LogLevel {
    return level_;
}

void FileTarget::flush() {
    if (file_) {
        std::fflush(file_);
    }
}

} // namespace mnesso::loggers
