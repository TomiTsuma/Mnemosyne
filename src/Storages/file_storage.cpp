// src/Storages/file_storage.cpp — File storage engine implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Storages/file_storage.h"
#include "Core/block.h"
#include "Columns/column_vector.h"
#include "Columns/column_string.h"
#include "DataTypes/data_type.h"
#include "Disks/disk.h"
#include "Disks/disk_local.h"
#include "Common/exceptions.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <sstream>

namespace mnemo::storages {

// ── Column serialization helpers ──────────────────────────────────────────────
//
// Binary format per column file:
//
//   Numeric columns (ColumnVector<T>):
//     [uint64_t row_count][T data[row_count]]
//
//   String / other columns (ColumnString):
//     [uint64_t row_count]
//     for each row: [uint32_t len][char bytes[len]]
//
// The type stored here perfectly matches what get_data_type(name) reconstructs,
// so pack round-trips without loss.

static auto make_typed_column(const datatypes::DataTypePtr& dt) -> core::ColumnPtr
{
    switch (dt->id()) {
        case datatypes::TypeId::UInt8:   return std::make_shared<columns::ColumnVector<uint8_t>>();
        case datatypes::TypeId::UInt16:  return std::make_shared<columns::ColumnVector<uint16_t>>();
        case datatypes::TypeId::UInt32:  return std::make_shared<columns::ColumnVector<uint32_t>>();
        case datatypes::TypeId::UInt64:  return std::make_shared<columns::ColumnVector<uint64_t>>();
        case datatypes::TypeId::Int8:    return std::make_shared<columns::ColumnVector<int8_t>>();
        case datatypes::TypeId::Int16:   return std::make_shared<columns::ColumnVector<int16_t>>();
        case datatypes::TypeId::Int32:   return std::make_shared<columns::ColumnVector<int32_t>>();
        case datatypes::TypeId::Int64:   return std::make_shared<columns::ColumnVector<int64_t>>();
        case datatypes::TypeId::Float32: return std::make_shared<columns::ColumnVector<float>>();
        case datatypes::TypeId::Float64: return std::make_shared<columns::ColumnVector<double>>();
        case datatypes::TypeId::Bool:    return std::make_shared<columns::ColumnVector<uint8_t>>();
        default:                          return columns::ColumnString::create();
    }
}

static bool is_string_type(datatypes::TypeId id)
{
    return id == datatypes::TypeId::String
        || id == datatypes::TypeId::FixedString
        || id == datatypes::TypeId::Date
        || id == datatypes::TypeId::DateTime
        || id == datatypes::TypeId::UUID
        || id == datatypes::TypeId::Array
        || id == datatypes::TypeId::Unknown;
}

static void serialize_column(const core::IColumn& col,
                              const datatypes::DataTypePtr& dt,
                              std::vector<uint8_t>& out)
{
    const uint64_t n = col.size();
    // Write row count header
    out.insert(out.end(),
               reinterpret_cast<const uint8_t*>(&n),
               reinterpret_cast<const uint8_t*>(&n) + sizeof(n));

    if (is_string_type(dt->id())) {
        for (uint64_t i = 0; i < n; ++i) {
            auto f = col.get(i);
            auto sv = f.as_string();
            std::string str = sv ? *sv : "";
            uint32_t len = static_cast<uint32_t>(str.size());
            out.insert(out.end(),
                       reinterpret_cast<uint8_t*>(&len),
                       reinterpret_cast<uint8_t*>(&len) + sizeof(len));
            out.insert(out.end(), str.begin(), str.end());
        }
    } else {
        // Numeric: use pack() which writes the raw T[] array
        col.pack(out);
    }
}

static void deserialize_column(core::IColumn& col,
                                const datatypes::DataTypePtr& dt,
                                std::span<const uint8_t> data)
{
    if (data.size() < sizeof(uint64_t)) return;

    uint64_t row_count = 0;
    std::memcpy(&row_count, data.data(), sizeof(row_count));
    const uint8_t* ptr = data.data() + sizeof(uint64_t);
    const uint8_t* end = data.data() + data.size();

    if (is_string_type(dt->id())) {
        for (uint64_t i = 0; i < row_count; ++i) {
            if (ptr + sizeof(uint32_t) > end) break;
            uint32_t len = 0;
            std::memcpy(&len, ptr, sizeof(len));
            ptr += sizeof(len);
            if (ptr + len > end) break;
            std::string str(reinterpret_cast<const char*>(ptr), len);
            ptr += len;
            col.insert(core::Field(std::move(str)));
        }
    } else {
        // Numeric: use unpack() with the bytes after the header
        const size_t remaining = static_cast<size_t>(end - ptr);
        col.unpack(std::span<const uint8_t>(ptr, remaining));
    }
}

// ── FileStorage ───────────────────────────────────────────────────────────────

auto FileStorage::create(std::string name, std::string path) -> std::shared_ptr<FileStorage>
{
    auto storage = std::shared_ptr<FileStorage>(new FileStorage());
    storage->name_ = std::move(name);
    storage->path_ = std::move(path);
    return storage;
}

FileStorage::FileStorage() = default;

auto FileStorage::name()         const -> std::string { return name_; }
auto FileStorage::engine()       const -> std::string { return "File"; }
auto FileStorage::path()         const -> std::string { return path_; }
auto FileStorage::file_path()    const -> std::string { return path_; }
auto FileStorage::is_temporary() const -> bool        { return false; }

auto FileStorage::columns() const -> std::vector<std::string>
{
    return column_names_;
}

auto FileStorage::column_types() const
    -> std::unordered_map<std::string, datatypes::DataTypePtr>
{
    return column_types_;
}

auto FileStorage::read(const std::vector<std::string>& col_names,
                       size_t /*max_block_size*/) -> core::Block
{
    std::lock_guard lock(mutex_);

    core::Block result;
    for (const auto& col_name : col_names) {
        auto type_it = column_types_.find(col_name);
        if (type_it == column_types_.end()) continue;

        auto col = make_typed_column(type_it->second);

        const auto file_path = path_ + "/" + col_name + ".bin";
        if (disk_ && disk_->exists(file_path)) {
            const auto file_size = disk_->size(file_path);
            if (file_size > 0) {
                auto raw = disk_->read(file_path, 0, file_size);
                if (raw) {
                    deserialize_column(*col, type_it->second,
                                       std::span<const uint8_t>(raw.get(), file_size));
                }
            }
        }
        result.add_column(col_name, col);
    }
    return result;
}

auto FileStorage::write(const core::Block& block) -> bool
{
    std::lock_guard lock(mutex_);

    // Ensure we have a disk — fall back to local disk rooted at path_ if none set
    if (!disk_) {
        auto dir = path_.substr(0, path_.find_last_of('/'));
        if (dir.empty()) dir = ".";
        disk_ = disks::LocalFileDisk::create("local", dir);
    }
    if (disk_ && !path_.empty()) {
        disk_->create_dir(path_);
    }

    // column_names returns by value, so it must outlive the loop below.
    const auto block_column_names = block.column_names();

    size_t appended_rows = 0;
    for (size_t c = 0; c < block.column_count(); ++c) {
        const auto& col_name = block_column_names[c];
        auto new_col = block.get_column_by_index(c);
        if (!new_col || new_col->size() == 0) continue;

        // Register column if unknown
        auto type_it = column_types_.find(col_name);
        if (type_it == column_types_.end()) {
            add_column(col_name, new_col->get_data_type());
            type_it = column_types_.find(col_name);
        }
        const auto& dt = type_it->second;

        const auto file_path = path_ + "/" + col_name + ".bin";

        // Load existing rows so we can append
        auto existing = make_typed_column(dt);
        if (disk_ && disk_->exists(file_path)) {
            const auto file_size = disk_->size(file_path);
            if (file_size > 0) {
                auto raw = disk_->read(file_path, 0, file_size);
                if (raw) {
                    deserialize_column(*existing, dt,
                                       std::span<const uint8_t>(raw.get(), file_size));
                }
            }
        }

        // Append new rows
        existing->insert_range(*new_col, 0, new_col->size());

        // Reserialize the merged column
        std::vector<uint8_t> packed;
        serialize_column(*existing, dt, packed);
        if (disk_) {
            disk_->write(file_path, std::span(packed.data(), packed.size()));
        }

        appended_rows = new_col->size();
        byte_count_ += appended_rows * dt->data_size();
    }

    row_count_ += appended_rows;
    empty_ = (row_count_ == 0);
    return true;
}

auto FileStorage::sync_row_count() -> void
{
    std::lock_guard lock(mutex_);
    if (!disk_ || column_names_.empty()) return;

    // Determine row count from the first column file
    const auto& first_col = column_names_.front();
    const auto file_path  = path_ + "/" + first_col + ".bin";
    if (!disk_->exists(file_path)) return;

    const auto file_size = disk_->size(file_path);
    if (file_size < sizeof(uint64_t)) return;

    auto raw = disk_->read(file_path, 0, sizeof(uint64_t));
    if (!raw) return;

    uint64_t n = 0;
    std::memcpy(&n, raw.get(), sizeof(n));
    row_count_ = static_cast<size_t>(n);
    empty_     = (row_count_ == 0);
}

auto FileStorage::write_schema(const std::string& db_name,
                                const std::string& table_name) -> void
{
    if (!disk_) return;

    std::ostringstream oss;
    oss << "{\"version\":1"
        << ",\"database\":\"" << db_name << "\""
        << ",\"table\":\""    << table_name << "\""
        << ",\"columns\":[";
    for (size_t i = 0; i < column_names_.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& n = column_names_[i];
        const auto& t = column_types_.at(n)->name();
        oss << "{\"name\":\"" << n << "\",\"type\":\"" << t << "\"}";
    }
    oss << "]}";

    const std::string json = oss.str();
    std::vector<uint8_t> bytes(json.begin(), json.end());
    disk_->write(path_ + "/_schema.json", std::span(bytes.data(), bytes.size()));
}

auto FileStorage::empty()      const -> bool   { return empty_; }
auto FileStorage::row_count()  const -> size_t { std::lock_guard lock(mutex_); return row_count_; }
auto FileStorage::byte_count() const -> size_t { std::lock_guard lock(mutex_); return byte_count_; }

auto FileStorage::alter(std::function<void(IStorage& storage)> modify) -> bool
{
    modify(*this);
    return true;
}

auto FileStorage::get_setting(std::string_view /*name*/)
    -> std::optional<common::SettingValueType>
{
    return std::nullopt;
}

auto FileStorage::lock()   -> bool { return mutex_.try_lock(); }
auto FileStorage::unlock() -> void {}
auto FileStorage::flush()  -> bool { return true; }

auto FileStorage::add_column(std::string name, datatypes::DataTypePtr type) -> void
{
    if (column_types_.find(name) == column_types_.end()) {
        column_names_.push_back(name);
    }
    column_types_[name] = std::move(type);
}

auto FileStorage::set_columns(
    std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void
{
    column_types_ = std::move(types);
    column_names_.clear();
    for (const auto& [name, _] : column_types_) {
        column_names_.push_back(name);
    }
}

auto FileStorage::set_columns_ordered(
    const std::vector<std::pair<std::string, datatypes::DataTypePtr>>& cols) -> void
{
    column_names_.clear();
    column_types_.clear();
    for (const auto& [name, type] : cols) {
        column_names_.push_back(name);
        column_types_[name] = type;
    }
}

auto FileStorage::set_disk(std::shared_ptr<disks::IDisk> disk) -> void
{
    std::lock_guard lock(mutex_);
    disk_ = std::move(disk);
}

auto FileStorage::get_disk() -> std::shared_ptr<disks::IDisk>
{
    std::lock_guard lock(mutex_);
    return disk_;
}

auto FileStorage::set_data_path(std::string path) -> void
{
    std::lock_guard lock(mutex_);
    path_ = std::move(path);
}

auto FileStorage::set_storage_unit_name(std::string name) -> void
{
    std::lock_guard lock(mutex_);
    storage_unit_name_ = std::move(name);
}

auto FileStorage::storage_unit_name() const -> std::string
{
    std::lock_guard lock(mutex_);
    return storage_unit_name_;
}

auto FileStorage::set_replica_group_name(std::string name) -> void
{
    std::lock_guard lock(mutex_);
    replica_group_name_ = std::move(name);
}

auto FileStorage::replica_group_name() const -> std::string
{
    std::lock_guard lock(mutex_);
    return replica_group_name_;
}

auto FileStorage::set_shard_group_name(std::string name) -> void
{
    std::lock_guard lock(mutex_);
    shard_group_name_ = std::move(name);
}

auto FileStorage::shard_group_name() const -> std::string
{
    std::lock_guard lock(mutex_);
    return shard_group_name_;
}

} // namespace mnemo::storages
