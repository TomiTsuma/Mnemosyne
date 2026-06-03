// src/Disks/disk_local.cpp — LocalFileDisk implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Disks/disk_local.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace mnesso::disks {

auto LocalFileDisk::create(std::string name, std::string path)
    -> std::shared_ptr<LocalFileDisk> {
    auto disk = std::shared_ptr<LocalFileDisk>(new LocalFileDisk());
    disk->name_ = std::move(name);
    disk->path_ = std::move(path);
    return disk;
}

LocalFileDisk::LocalFileDisk() = default;

auto LocalFileDisk::name() const -> std::string { return name_; }
auto LocalFileDisk::path() const -> std::string { return path_; }
auto LocalFileDisk::type() const -> std::string { return "Local"; }

auto LocalFileDisk::exists(std::string_view path) -> bool {
    std::lock_guard lock(mutex_);
    return std::filesystem::exists(path_ + "/" + std::string{path});
}

auto LocalFileDisk::size(std::string_view path) -> size_t {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    struct stat st;
    if (stat(full_path.c_str(), &st) != 0) return 0;
    return static_cast<size_t>(st.st_size);
}

auto LocalFileDisk::modified_at(std::string_view path)
    -> std::chrono::system_clock::time_point {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    struct stat st;
    if (stat(full_path.c_str(), &st) != 0) return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(st.st_mtime);
}

auto LocalFileDisk::read(std::string_view path, size_t offset,
                         size_t size) -> std::shared_ptr<uint8_t[]> {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};

    std::ifstream file(full_path, std::ios::binary);
    if (!file) return nullptr;

    file.seekg(0, std::ios::end);
    std::streamoff file_size_off = file.tellg();
    if (file_size_off < 0) return nullptr;
    auto file_size = static_cast<size_t>(file_size_off);
    if (offset >= file_size) return nullptr;

    auto read_size = size < (file_size - offset) ? size : (file_size - offset);

    // allocate a shared_ptr for an array using default_delete for arrays
    auto data = std::shared_ptr<uint8_t[]>(new uint8_t[read_size], std::default_delete<uint8_t[]>());

    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(data.get()), static_cast<std::streamsize>(read_size));

    return data;
}

auto LocalFileDisk::write(std::string_view path, std::span<const uint8_t> data) -> bool {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};

    // Create parent directory if needed
    auto parent = std::filesystem::path(full_path).parent_path();
    std::filesystem::create_directories(parent);

    std::ofstream file(full_path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

auto LocalFileDisk::rename(std::string_view from, std::string_view to) -> bool {
    std::lock_guard lock(mutex_);
    auto from_path = path_ + "/" + std::string{from};
    auto to_path = path_ + "/" + std::string{to};
    std::error_code ec;
    std::filesystem::rename(from_path, to_path, ec);
    return !ec;
}

auto LocalFileDisk::remove(std::string_view path) -> bool {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    std::error_code ec;
    return std::filesystem::remove(full_path, ec);
}

auto LocalFileDisk::list_dirs(std::string_view path)
    -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    std::vector<std::string> dirs;

    if (std::filesystem::is_directory(full_path)) {
        for (auto& entry : std::filesystem::directory_iterator(full_path)) {
            if (entry.is_directory()) {
                dirs.push_back(entry.path().filename().string());
            }
        }
    }
    return dirs;
}

auto LocalFileDisk::list_files(std::string_view path)
    -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    std::vector<std::string> files;

    if (std::filesystem::is_directory(full_path)) {
        for (auto& entry : std::filesystem::directory_iterator(full_path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    return files;
}

auto LocalFileDisk::create_dir(std::string_view path) -> bool {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    std::error_code ec;
    bool created = std::filesystem::create_directories(full_path, ec);
    if (created) {
        return true;
    }
    return std::filesystem::exists(full_path, ec);
}

auto LocalFileDisk::remove_dir(std::string_view path) -> bool {
    std::lock_guard lock(mutex_);
    auto full_path = path_ + "/" + std::string{path};
    return std::filesystem::remove_all(full_path) > 0;
}

auto LocalFileDisk::stats() const -> DiskStats {
    DiskStats s;
    std::error_code ec;
    auto space_info = std::filesystem::space(path_, ec);
    if (!ec) {
        s.free_space = static_cast<uint64_t>(space_info.available);
        s.total_space = static_cast<uint64_t>(space_info.capacity);
        s.space_used = s.total_space > s.free_space ? s.total_space - s.free_space : 0;
        s.space_used_pct = s.total_space > 0 ?
            static_cast<double>(s.space_used) / s.total_space * 100.0 : 0.0;
    }
    return s;
}

// get_raw_ptr is not part of the LocalFileDisk public interface — do not define it here.

} // namespace mnesso::disks
