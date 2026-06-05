// src/Backups/backup.cpp — Backup layer implementation

#include "backup.h"
#include "Databases/database_manager.h"
#include "Storages/memory_storage.h"
#include "IO/codecs.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace mnemo::backups {

namespace {

auto make_backup_name(std::string_view database, std::string_view table) -> std::string {
    return std::string{database} + "_" + std::string{table};
}

auto make_database_backup_dir(std::string_view database) -> std::string {
    return std::string{database} + "_full";
}

[[nodiscard]] auto read_table_block(databases::DatabaseManager& manager,
                                    std::string_view database,
                                    std::string_view table) -> core::Block {
    const auto storage = manager.get_table_storage(database, table);
    if (!storage) {
        throw common::Exception{
            "Backup: table '" + std::string{table} + "' not found in database '" +
                std::string{database} + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    const auto column_names = storage->columns();
    if (column_names.empty()) {
        return {};
    }
    return storage->read(column_names);
}

void restore_block_to_storage(const std::shared_ptr<storages::IStorage>& storage,
                              const core::Block& block) {
    if (auto memory = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        memory->load_block(block);
        return;
    }

    if (storage->empty()) {
        if (!storage->write(block)) {
            throw common::Exception{
                "Backup: failed to write restored data to storage '" + storage->name() + "'",
                static_cast<int>(common::ErrorCode::STORAGE_ERROR)};
        }
        return;
    }

    throw common::Exception{
        "Backup: cannot restore into non-empty storage '" + storage->name() +
            "' (only MemoryStorage supports replace)",
        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

void encode_block_to_stream(const core::Block& block, std::ostream& out) {
    std::vector<uint8_t> encoded;
    io::BinaryCodec::instance().encode(block, encoded);
    out.write(reinterpret_cast<const char*>(encoded.data()),
              static_cast<std::streamsize>(encoded.size()));
}

[[nodiscard]] auto decode_block_from_bytes(const std::vector<uint8_t>& data) -> core::Block {
    if (data.empty()) {
        return {};
    }
    return io::BinaryCodec::instance().decode({data.data(), data.size()});
}

} // namespace

auto Backup::create(std::string name, std::string path) -> std::shared_ptr<Backup> {
    auto backup = std::shared_ptr<Backup>(new Backup());
    backup->name_ = std::move(name);
    backup->path_ = std::move(path);
    std::filesystem::create_directories(backup->path_);
    return backup;
}

Backup::Backup() = default;

auto Backup::start() -> bool { return true; }

auto Backup::stop() -> bool { return true; }

auto Backup::backup_file_path(std::string_view backup_name) const -> std::string {
    return (std::filesystem::path(path_) / (std::string{backup_name} + ".bin")).string();
}

auto Backup::database_backup_dir(std::string_view database) const -> std::string {
    return (std::filesystem::path(path_) / make_database_backup_dir(database)).string();
}

auto Backup::table_backup_file_path(std::string_view database, std::string_view table) const
    -> std::string {
    return (std::filesystem::path(database_backup_dir(database)) / (std::string{table} + ".bin"))
        .string();
}

auto Backup::write_block_to_file(std::string_view backup_name, const core::Block& block)
    -> BackupInfo {
    const auto file_path = backup_file_path(backup_name);
    std::filesystem::create_directories(std::filesystem::path(file_path).parent_path());

    std::ofstream out(file_path, std::ios::binary);
    if (!out) {
        throw common::Exception{
            "Backup::write_block_to_file: cannot open " + file_path,
            static_cast<int>(common::ErrorCode::CANNOT_OPEN_FILE)};
    }

    encode_block_to_stream(block, out);
    if (!out) {
        throw common::Exception{
            "Backup::write_block_to_file: write failed for " + file_path,
            static_cast<int>(common::ErrorCode::CANNOT_WRITE)};
    }

    const auto size = std::filesystem::file_size(file_path);

    return BackupInfo{
        std::string{backup_name},
        file_path,
        std::chrono::system_clock::now(),
        size,
        {},
        "CREATED",
    };
}

auto Backup::write_block_to_path(const std::string& file_path, const core::Block& block) -> size_t {
    std::filesystem::create_directories(std::filesystem::path(file_path).parent_path());

    std::ofstream out(file_path, std::ios::binary);
    if (!out) {
        throw common::Exception{
            "Backup::write_block_to_path: cannot open " + file_path,
            static_cast<int>(common::ErrorCode::CANNOT_OPEN_FILE)};
    }

    encode_block_to_stream(block, out);
    if (!out) {
        throw common::Exception{
            "Backup::write_block_to_path: write failed for " + file_path,
            static_cast<int>(common::ErrorCode::CANNOT_WRITE)};
    }

    return std::filesystem::file_size(file_path);
}

auto Backup::read_block_from_file(std::string_view backup_name) const -> core::Block {
    return read_block_from_path(backup_file_path(backup_name));
}

auto Backup::read_block_from_path(const std::string& file_path) const -> core::Block {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        throw common::Exception{
            "Backup::read_block_from_path: cannot open " + file_path,
            static_cast<int>(common::ErrorCode::CANNOT_OPEN_FILE)};
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    return decode_block_from_bytes(data);
}

void Backup::upsert_backup_info(BackupInfo info) {
    auto it = std::find_if(backups_.begin(), backups_.end(),
                           [&](const BackupInfo& b) { return b.name == info.name; });
    if (it != backups_.end()) {
        *it = std::move(info);
    } else {
        backups_.push_back(std::move(info));
    }
}

auto Backup::backup_table(std::string database, std::string table) -> bool {
    std::lock_guard lock(mutex_);

    auto& manager = databases::DatabaseManager::instance();
    if (!manager.has_database(database)) {
        throw common::Exception{
            "Backup::backup_table: unknown database '" + database + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }

    const auto block = read_table_block(manager, database, table);
    const auto backup_name = make_backup_name(database, table);

    auto info = write_block_to_file(backup_name, block);
    info.tables = {std::move(table)};
    upsert_backup_info(std::move(info));
    return true;
}

auto Backup::restore_table(std::string database, std::string table) -> bool {
    std::lock_guard lock(mutex_);

    const auto backup_name = make_backup_name(database, table);
    if (!std::filesystem::exists(backup_file_path(backup_name))) {
        throw common::Exception{
            "Backup::restore_table: backup not found: " + backup_name,
            static_cast<int>(common::ErrorCode::CANNOT_READ)};
    }

    auto& manager = databases::DatabaseManager::instance();
    const auto storage = manager.get_table_storage(database, table);
    if (!storage) {
        throw common::Exception{
            "Backup::restore_table: table '" + table + "' not found in database '" + database + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    const auto block = read_block_from_file(backup_name);
    restore_block_to_storage(storage, block);
    return true;
}

auto Backup::backup_database(std::string database) -> bool {
    std::lock_guard lock(mutex_);

    auto& manager = databases::DatabaseManager::instance();
    if (!manager.has_database(database)) {
        throw common::Exception{
            "Backup::backup_database: unknown database '" + database + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }

    const auto table_names = manager.list_tables(database);
    const auto backup_dir = database_backup_dir(database);
    std::filesystem::create_directories(backup_dir);

    size_t total_bytes = 0;
    for (const auto& table : table_names) {
        const auto block = read_table_block(manager, database, table);
        total_bytes += write_block_to_path(table_backup_file_path(database, table), block);
    }

    BackupInfo info;
    info.name = make_database_backup_dir(database);
    info.path = backup_dir;
    info.created_at = std::chrono::system_clock::now();
    info.size_bytes = total_bytes;
    info.tables = table_names;
    info.status = "CREATED";
    upsert_backup_info(std::move(info));
    return true;
}

auto Backup::restore_database(std::string database) -> bool {
    std::lock_guard lock(mutex_);

    const auto backup_dir = database_backup_dir(database);
    if (!std::filesystem::is_directory(backup_dir)) {
        throw common::Exception{
            "Backup::restore_database: backup not found: " + backup_dir,
            static_cast<int>(common::ErrorCode::CANNOT_READ)};
    }

    auto& manager = databases::DatabaseManager::instance();
    if (!manager.has_database(database)) {
        throw common::Exception{
            "Backup::restore_database: unknown database '" + database + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }

    for (const auto& entry : std::filesystem::directory_iterator(backup_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".bin") {
            continue;
        }

        const auto table = entry.path().stem().string();
        const auto storage = manager.get_table_storage(database, table);
        if (!storage) {
            throw common::Exception{
                "Backup::restore_database: table '" + table + "' not found in database '" +
                    database + "'",
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }

        const auto block = read_block_from_path(entry.path().string());
        restore_block_to_storage(storage, block);
    }

    return true;
}

auto Backup::list() const -> std::vector<BackupInfo> {
    std::lock_guard lock(mutex_);
    return backups_;
}

auto Backup::info(std::string_view name) const -> std::optional<BackupInfo> {
    std::lock_guard lock(mutex_);
    auto it = std::find_if(backups_.begin(), backups_.end(),
                           [&](const BackupInfo& b) { return b.name == name; });
    if (it == backups_.end()) {
        return std::nullopt;
    }
    return *it;
}

auto Backup::remove(std::string_view name) -> bool {
    std::lock_guard lock(mutex_);
    auto it = std::find_if(backups_.begin(), backups_.end(),
                           [&](const BackupInfo& b) { return b.name == name; });
    if (it == backups_.end()) {
        return false;
    }

    std::error_code ec;
    if (std::filesystem::is_directory(it->path)) {
        std::filesystem::remove_all(it->path, ec);
    } else {
        std::filesystem::remove(it->path, ec);
    }
    backups_.erase(it);
    return true;
}

auto Backup::disk_usage() const -> size_t {
    std::lock_guard lock(mutex_);
    size_t total = 0;
    for (const auto& backup : backups_) {
        total += backup.size_bytes;
    }
    return total;
}

} // namespace mnemo::backups
