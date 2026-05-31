// src/Disks/disk_local.h — LocalFileDisk: POSIX / Win32 filesystem
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "disk.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <cstdint>

namespace mnesso::disks {

// ── LocalFileDisk — standard filesystem back-end ──
class LocalFileDisk final : public IDisk {
public:
    static auto create(std::string name, std::string path)
        -> std::shared_ptr<LocalFileDisk>;

    // IDisk interface
    [[nodiscard]] auto name()      const -> std::string override;
    [[nodiscard]] auto path()      const -> std::string override;
    [[nodiscard]] auto type()      const -> std::string override;

    [[nodiscard]] auto exists(std::string_view path) -> bool override;
    [[nodiscard]] auto size(std::string_view path) -> size_t override;
    [[nodiscard]] auto modified_at(std::string_view path)
        -> std::chrono::system_clock::time_point override;

    [[nodiscard]] auto read(std::string_view path, size_t offset,
                            size_t size) -> std::shared_ptr<uint8_t[]> override;
    auto write(std::string_view path, std::span<const uint8_t> data)
        -> bool override;

    auto rename(std::string_view from, std::string_view to) -> bool override;
    auto remove(std::string_view path) -> bool override;

    [[nodiscard]] auto list_dirs(std::string_view path)
        -> std::vector<std::string> override;
    [[nodiscard]] auto list_files(std::string_view path)
        -> std::vector<std::string> override;
    auto create_dir(std::string_view path) -> bool override;
    auto remove_dir(std::string_view path) -> bool override;

    [[nodiscard]] auto stats() const -> DiskStats override;

protected:
    LocalFileDisk();

private:
    std::string name_;
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace mnesso::disks
