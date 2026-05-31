// src/Backups/backup.h — Backup manager for database snapshots
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mnesso::backups {

// ── Backup info ──
struct BackupInfo {
    std::string                           name;
    std::string                           path;
    std::chrono::system_clock::time_point created_at{};
    std::uintmax_t                        size_bytes = 0;
    std::vector<std::string>              tables;
    std::string                           status = "CREATED"; // CREATED, UPLOADING, ERROR
};

// ── Backup — backup manager ──
class Backup {
public:
    static auto create(std::string name, std::string path) -> std::shared_ptr<Backup>;

    auto start() -> bool;
    auto stop() -> bool;

    auto backup_table(std::string database, std::string table) -> bool;
    auto restore_table(std::string database, std::string table) -> bool;
    auto backup_database(std::string database) -> bool;
    auto restore_database(std::string database) -> bool;

    [[nodiscard]] auto list() const -> std::vector<BackupInfo>;
    [[nodiscard]] auto info(std::string_view name) const -> std::optional<BackupInfo>;
    auto remove(std::string_view name) -> bool;

    [[nodiscard]] auto disk_usage() const -> size_t;

private:
    Backup();

    [[nodiscard]] auto backup_file_path(std::string_view backup_name) const -> std::string;
    [[nodiscard]] auto database_backup_dir(std::string_view database) const -> std::string;
    [[nodiscard]] auto table_backup_file_path(std::string_view database,
                                              std::string_view table) const -> std::string;

    auto write_block_to_file(std::string_view backup_name, const core::Block& block) -> BackupInfo;
    [[nodiscard]] auto write_block_to_path(const std::string& file_path,
                                           const core::Block& block) -> size_t;
    [[nodiscard]] auto read_block_from_file(std::string_view backup_name) const -> core::Block;
    [[nodiscard]] auto read_block_from_path(const std::string& file_path) const -> core::Block;

    void upsert_backup_info(BackupInfo info);

    std::string               name_;
    std::string               path_;
    std::vector<BackupInfo>   backups_;
    mutable std::mutex        mutex_;
};

} // namespace mnesso::backups
