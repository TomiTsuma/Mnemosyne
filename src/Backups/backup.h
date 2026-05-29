// src/Backups/backup.h — Backup manager for database snapshots
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <chrono>

namespace mnesso::backups {

// ── Backup info ──
struct BackupInfo {
    std::string name;
    std::string path;
    std::chrono::system_clock::time_point created_at;
    size_t                                size_bytes;
    std::vector<std::string>              tables;
    std::string                           status; // CREATED, UPLOADING, ERROR
};

// ── Backup — backup manager ──
class Backup {
public:
    static auto create(std::string name, std::string path)
        -> std::shared_ptr<Backup>;

    // Lifecycle
    auto start() -> bool;
    auto stop() -> bool;

    // Backup operations
    auto backup_table(std::string database, std::string table) -> bool;
    auto restore_table(std::string database, std::string table) -> bool;
    auto backup_database(std::string database) -> bool;
    auto restore_database(std::string database) -> bool;

    // List / delete backups
    [[nodiscard]] auto list() const -> std::vector<BackupInfo>;
    [[nodiscard]] auto info(std::string name) -> std::optional<BackupInfo>;
    auto remove(std::string name) -> bool;

    // Check disk space
    [[nodiscard]] auto disk_usage() const -> size_t;

private:
    Backup();
    std::string          name_;
    std::string          path_;
    std::vector<BackupInfo> backups_;
    mutable std::mutex     mutex_;
};

} // namespace mnesso::backups
