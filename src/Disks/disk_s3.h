// src/Disks/disk_s3.h — S3Disk: object storage back-end (S3, MinIO, etc.)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "disk.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace mnemo::disks {

// ── S3Disk — S3-compatible object store ──
class S3Disk final : public IDisk {
public:
    static auto create(std::string name,
                       std::string endpoint,
                       std::string bucket,
                       std::string access_key,
                       std::string secret_key,
                       std::string region = "")
        -> std::shared_ptr<S3Disk>;

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

    // S3-specific
    auto set_endpoint(std::string endpoint) -> void;
    auto set_bucket(std::string bucket) -> void;

private:
    S3Disk();

    // Builds the exact request-target string (path + optional query string)
    // used both to sign a request and to send it, so the two always match.
    [[nodiscard]] auto object_path(std::string_view key) const -> std::string;
    // The exact value sent (and signed) as the "Host" header.
    [[nodiscard]] auto host_header() const -> std::string;

    std::string name_;
    std::string endpoint_;
    std::string bucket_;
    std::string access_key_;
    std::string secret_key_;
    std::string region_ = "us-east-1";
};

} // namespace mnemo::disks
