// src/Storages/file_storage.cpp — File storage engine implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Storages/file_storage.h"
#include "Core/block.h"
#include "Columns/column_vector.h"
#include "Disks/disk.h"
#include "Disks/disk_local.h"
#include "Common/exceptions.h"
#include <fstream>
#include <algorithm>

namespace mnemo::storages {

auto FileStorage::create(std::string name, std::string path) -> std::shared_ptr<FileStorage> {
    auto storage = std::shared_ptr<FileStorage>(new FileStorage());
    storage->name_ = std::move(name);
    storage->path_ = std::move(path);
    return storage;
}

FileStorage::FileStorage() = default;

auto FileStorage::name() const -> std::string { return name_; }
auto FileStorage::engine() const -> std::string { return "File"; }
auto FileStorage::path() const -> std::string { return path_; }
auto FileStorage::is_temporary() const -> bool { return false; }

auto FileStorage::columns() const -> std::vector<std::string> {
    return column_names_;
}

auto FileStorage::column_types() const -> std::unordered_map<std::string, datatypes::DataTypePtr> {
    return column_types_;
}



auto FileStorage::read(const std::vector<std::string>& column_names,
                       size_t max_block_size) -> core::Block {
    std::lock_guard lock(mutex_);

    core::Block result;
    for (auto& col_name : column_names) {
        auto it = column_types_.find(col_name);
        if (it == column_types_.end()) continue;

        // Read column data from file
        auto file_path = path_ + "/" + col_name + ".bin";
        if (!disk_ || !disk_->exists(file_path)) continue;

        auto file_size = disk_->size(file_path);
        auto data = disk_->read(file_path, 0, file_size);

        auto col = std::make_shared<columns::ColumnVector<double>>();
        if (data) {
            size_t num_rows = file_size / sizeof(double);
            for (size_t i = 0; i < num_rows; ++i) {
                double val = *reinterpret_cast<double*>(data.get() + i * sizeof(double));
                (void)col->insert_at(i, core::Field{val});
            }
        }
        result.add_column(col_name, col);
    }
    return result;
}

auto FileStorage::write(const core::Block& block) -> bool {
    std::lock_guard lock(mutex_);

    for (size_t c = 0; c < block.column_count(); ++c) {
        auto col_name = block.column_names()[c];
        auto col = block.get_column_by_index(c);
        if (!col) continue;

        auto it = column_types_.find(col_name);
        if (it == column_types_.end()) {
            add_column(col_name, col->get_data_type());
        } else if (std::find(column_names_.begin(), column_names_.end(), col_name) == column_names_.end()) {
            column_names_.push_back(col_name);
        }

        // Write column data to file
        auto file_path = path_ + "/" + col_name + ".bin";
        std::vector<uint8_t> data;
        size_t num_rows = col->size();
        data.resize(num_rows * sizeof(double));
        for (size_t i = 0; i < num_rows; ++i) {
            const auto& val = col->get_at(i);
            double d = 0.0;
            if (auto fv = val.as_float64()) {
                d = *fv;
            } else if (auto iv = val.as_int64()) {
                d = static_cast<double>(*iv);
            }
            *reinterpret_cast<double*>(data.data() + i * sizeof(double)) = d;
        }

        if (!disk_) {
            // Create local disk
            auto dir = path_.substr(0, path_.find_last_of('/'));
            if (!dir.empty()) {
                disk_ = disks::LocalFileDisk::create("local", dir);
            }
        }

        if (disk_) {
            disk_->write(file_path, std::span(data.data(), data.size()));
            byte_count_ += data.size();
            row_count_ = std::max(row_count_, num_rows);
        }
    }
    empty_ = false;
    return true;
}

auto FileStorage::empty() const -> bool { return empty_; }

auto FileStorage::row_count() const -> size_t {
    std::lock_guard lock(mutex_);
    return row_count_;
}

auto FileStorage::byte_count() const -> size_t {
    std::lock_guard lock(mutex_);
    return byte_count_;
}

auto FileStorage::alter(std::function<void(IStorage& storage)> modify) -> bool {
    modify(*this);
    return true;
}

auto FileStorage::get_setting(std::string_view name)
    -> std::optional<common::SettingValueType> {
    return std::nullopt;
}

auto FileStorage::lock() -> bool { return mutex_.try_lock(); }
auto FileStorage::unlock() -> void {}
auto FileStorage::flush() -> bool { return true; }

auto FileStorage::add_column(std::string name, datatypes::DataTypePtr type) -> void {
    column_names_.push_back(std::move(name));
    column_types_[column_names_.back()] = std::move(type);
}

auto FileStorage::set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void {
    column_types_ = std::move(types);
    column_names_.clear();
    for (auto& [name, _] : column_types_) {
        column_names_.push_back(name);
    }
}

} // namespace mnemo::storages
