// src/Loggers/target_file.h — File log target
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "logger.h"
#include <string>
#include <memory>
#include <mutex>

namespace mnemo::loggers {

// ── FileTarget — logs to a file ──
class FileTarget final : public LogTarget {
public:
    static auto create(std::string path) -> std::shared_ptr<FileTarget>;

    [[nodiscard]] auto name()     const -> std::string override;
    auto emit(const LogEntry& entry) -> bool override;

    void set_level(LogLevel level);
    [[nodiscard]] auto level() const -> LogLevel;

    // Flush to disk
    void flush();

private:
    FileTarget();
    std::string         path_;
    LogLevel            level_ = LogLevel::TRACE;
    std::FILE*         file_  = nullptr;
    std::mutex          mutex_;
};

} // namespace mnemo::loggers
