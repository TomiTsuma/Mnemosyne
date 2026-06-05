// src/Storages/file_storage.h — File storage engine
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_storage.h"
#include "Disks/disk.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>
#include "Common/settings.h"

namespace mnemo::storages {

// ── FileStorage — persists columns as files on disk ──
class FileStorage final : public IStorage {
public:
    static auto create(std::string name, std::string path) -> std::shared_ptr<FileStorage>;

    // IStorage interface
    [[nodiscard]] auto name()      const -> std::string override;
    [[nodiscard]] auto engine()    const -> std::string override;
    [[nodiscard]] auto path()      const -> std::string override;
    [[nodiscard]] auto is_temporary() const -> bool override;

    [[nodiscard]] auto columns()       const -> std::vector<std::string> override;
    auto column_types() const -> std::unordered_map<std::string, datatypes::DataTypePtr> override;
    [[nodiscard]] auto read(const std::vector<std::string>& column_names,
                            size_t max_block_size = 0) -> core::Block override;
    auto write(const core::Block& block) -> bool override;
    [[nodiscard]] auto empty() const -> bool override;
    [[nodiscard]] auto row_count()  const -> size_t override;
    [[nodiscard]] auto byte_count() const -> size_t override;

    auto alter(std::function<void(IStorage& storage)> modify) -> bool override;
    [[nodiscard]] auto get_setting(std::string_view name)
        -> std::optional<common::SettingValueType> override;
    auto lock()  -> bool override;
    auto unlock() -> void override;
    auto flush() -> bool override;

    // File-specific
    [[nodiscard]] auto file_path() const -> std::string;
    auto set_disk(std::shared_ptr<disks::IDisk> disk) -> void;
    auto get_disk() -> std::shared_ptr<disks::IDisk>;
    auto add_column(std::string name, datatypes::DataTypePtr type) -> void;
    auto set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void;

private:
    FileStorage();
    std::string name_;
    std::string path_;
    std::unordered_map<std::string, datatypes::DataTypePtr> column_types_;
    std::vector<std::string> column_names_;
    size_t row_count_ = 0;
    size_t byte_count_ = 0;
    bool empty_ = true;
    std::shared_ptr<disks::IDisk> disk_;
    bool    locked_ = false;
    mutable std::mutex mutex_;
};

} // namespace mnemo::storages
