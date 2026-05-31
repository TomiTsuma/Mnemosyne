// src/Disks/disk.h — IDisk: abstract disk back-end interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "Common/span_compat.h"
#include <cstdint>
#include <memory>
#include <chrono>

namespace mnesso::disks {

// ── Disk stats ──
struct DiskStats {
    uint64_t free_space     = 0;
    uint64_t total_space    = 0;
    uint64_t space_used     = 0;
    double   space_used_pct = 0.0;
};

// ── IDisk — abstract disk back-end ──
// All concrete disks (LocalDisk, S3Disk, etc.) implement this.
class IDisk {
public:
    virtual ~IDisk() = default;

    // Disk identification
    [[nodiscard]] virtual auto name()      const -> std::string = 0;
    [[nodiscard]] virtual auto path()      const -> std::string = 0;
    [[nodiscard]] virtual auto type()      const -> std::string = 0;

    // File operations
    [[nodiscard]] virtual auto exists(std::string_view path) -> bool = 0;
    [[nodiscard]] virtual auto size(std::string_view path) -> size_t = 0;
    [[nodiscard]] virtual auto modified_at(std::string_view path)
        -> std::chrono::system_clock::time_point = 0;

    // Read / write
    [[nodiscard]] virtual auto read(std::string_view path, size_t offset,
                                     size_t size) -> std::shared_ptr<uint8_t[]> = 0;
    virtual auto write(std::string_view path, std::span<const uint8_t> data)
        -> bool = 0;

    // Rename / delete
    virtual auto rename(std::string_view from, std::string_view to) -> bool = 0;
    virtual auto remove(std::string_view path) -> bool = 0;

    // Directory operations
    [[nodiscard]] virtual auto list_dirs(std::string_view path)
        -> std::vector<std::string> = 0;
    [[nodiscard]] virtual auto list_files(std::string_view path)
        -> std::vector<std::string> = 0;
    virtual auto create_dir(std::string_view path) -> bool = 0;
    virtual auto remove_dir(std::string_view path) -> bool = 0;

    // Stats
    [[nodiscard]] virtual auto stats() const -> DiskStats = 0;

    // Get underlying raw pointer (for zero-copy optimization)
    [[nodiscard]] virtual auto get_raw_ptr() const -> void*;
};

} // namespace mnesso::disks
