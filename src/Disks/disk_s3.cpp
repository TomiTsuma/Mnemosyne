// src/Disks/disk_s3.cpp — S3Disk placeholder implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Disks/disk_s3.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::disks {

auto S3Disk::create(std::string name,
                    std::string endpoint,
                    std::string bucket,
                    std::string access_key,
                    std::string secret_key,
                    bool use_ssl) -> std::shared_ptr<S3Disk> {
    auto disk = std::shared_ptr<S3Disk>(new S3Disk());
    disk->name_ = std::move(name);
    disk->endpoint_ = std::move(endpoint);
    disk->bucket_ = std::move(bucket);
    disk->access_key_ = std::move(access_key);
    disk->secret_key_ = std::move(secret_key);
    disk->use_ssl_ = use_ssl;
    return disk;
}

S3Disk::S3Disk() = default;

auto S3Disk::name() const -> std::string { return name_; }
auto S3Disk::path() const -> std::string { return bucket_; }
auto S3Disk::type() const -> std::string { return "S3"; }

auto S3Disk::exists(std::string_view path) -> bool {
    // Placeholder: would use S3 HeadObject API
    return false;
}

auto S3Disk::size(std::string_view path) -> size_t {
    // Placeholder: would use S3 HeadObject API
    return 0;
}

auto S3Disk::modified_at(std::string_view path)
    -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::now();
}

auto S3Disk::read(std::string_view path, size_t offset,
                  size_t size) -> std::shared_ptr<uint8_t[]> {
    // Placeholder: would use S3 GetObject API with Range header
    return nullptr;
}

auto S3Disk::write(std::string_view path, std::span<const uint8_t> data)
    -> bool {
    // Placeholder: would use S3 PutObject API
    return false;
}

auto S3Disk::rename(std::string_view from, std::string_view to) -> bool {
    // Placeholder: would use S3 CopyObject + DeleteObject
    return false;
}

auto S3Disk::remove(std::string_view path) -> bool {
    // Placeholder: would use S3 DeleteObject API
    return false;
}

auto S3Disk::list_dirs(std::string_view path)
    -> std::vector<std::string> {
    // Placeholder: would use S3 ListObjects with Delimiter
    return {};
}

auto S3Disk::list_files(std::string_view path)
    -> std::vector<std::string> {
    // Placeholder: would use S3 ListObjects
    return {};
}

auto S3Disk::create_dir(std::string_view path) -> bool {
    // S3 doesn't have directories, just keys
    return true;
}

auto S3Disk::remove_dir(std::string_view path) -> bool {
    // S3 doesn't have directories
    return false;
}

auto S3Disk::stats() const -> DiskStats {
    DiskStats s;
    // Placeholder
    return s;
}

auto S3Disk::set_endpoint(std::string endpoint) -> void {
    endpoint_ = std::move(endpoint);
}

auto S3Disk::set_bucket(std::string bucket) -> void {
    bucket_ = std::move(bucket);
}

} // namespace mnesso::disks
