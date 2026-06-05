// src/Storages/memory_storage.cpp — In-memory storage engine implementation

#include "Storages/memory_storage.h"
#include "Core/block.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::storages {

namespace {

[[nodiscard]] auto column_index(const std::vector<std::string>& names, std::string_view col_name)
    -> std::optional<size_t> {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == col_name) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace

auto MemoryStorage::create(std::string name) -> std::shared_ptr<MemoryStorage> {
    auto storage = std::shared_ptr<MemoryStorage>(new MemoryStorage());
    storage->name_ = std::move(name);
    return storage;
}

MemoryStorage::MemoryStorage() = default;

auto MemoryStorage::name() const -> std::string {
    std::lock_guard lock(mutex_);
    return name_;
}

auto MemoryStorage::engine() const -> std::string { return "Memory"; }
auto MemoryStorage::path() const -> std::string { return ""; }
auto MemoryStorage::is_temporary() const -> bool { return true; }

auto MemoryStorage::columns() const -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    return column_names_;
}

auto MemoryStorage::column_types() const -> std::unordered_map<std::string, datatypes::DataTypePtr> {
    std::lock_guard lock(mutex_);
    return column_types_;
}

auto MemoryStorage::read(const std::vector<std::string>& column_names,
                         size_t max_block_size) -> core::Block {
    std::lock_guard lock(mutex_);
    (void)max_block_size;

    core::Block result;
    for (const auto& col_name : column_names) {
        if (column_types_.find(col_name) == column_types_.end()) {
            continue;
        }

        const auto idx = column_index(column_names_, col_name);
        if (!idx) {
            continue;
        }

        const auto col = data_.get_column_by_index(*idx);
        if (!col) {
            continue;
        }

        result.add_column(col_name, col->clone());
    }
    return result;
}

auto MemoryStorage::write(const core::Block& block) -> bool {
    if (block.column_count() == 0) {
        return true;
    }

    std::lock_guard lock(mutex_);

    if (data_.column_count() == 0) {
        for (size_t c = 0; c < block.column_count(); ++c) {
            const auto col_name = block.column_names()[c];
            const auto col = block.get_column_by_index(c);
            if (!col) {
                continue;
            }
            if (column_types_.find(col_name) == column_types_.end()) {
                add_column_unlocked(col_name, col->get_data_type());
            }
        }
        data_ = block.clone();
        empty_ = false;
        return true;
    }

    for (size_t c = 0; c < block.column_count(); ++c) {
        const auto col_name = block.column_names()[c];
        const auto col = block.get_column_by_index(c);
        if (!col) {
            continue;
        }

        if (column_types_.find(col_name) == column_types_.end()) {
            add_column_unlocked(col_name, col->get_data_type());
        } else if (std::find(column_names_.begin(), column_names_.end(), col_name) == column_names_.end()) {
            column_names_.push_back(col_name);
        }

        auto existing = data_.get_column_by_name(col_name);
        if (!existing) {
            auto new_col = col->clone_empty();
            if (data_.row_count() > 0) {
                new_col->insert_many_default(data_.row_count());
            }
            data_.add_column(col_name, new_col);
            existing = data_.get_column_by_name(col_name);
        }

        for (size_t r = 0; r < col->size(); ++r) {
            (void)existing->insert_at(existing->size(), col->get_at(r));
        }
    }

    empty_ = false;
    data_ = data_.clone();
    return true;
}

auto MemoryStorage::empty() const -> bool {
    std::lock_guard lock(mutex_);
    return empty_;
}

auto MemoryStorage::row_count() const -> size_t {
    std::lock_guard lock(mutex_);
    return data_.row_count();
}

auto MemoryStorage::byte_count() const -> size_t {
    std::lock_guard lock(mutex_);
    size_t total = 0;
    for (const auto& col_name : column_names_) {
        const auto idx = column_index(column_names_, col_name);
        if (!idx) {
            continue;
        }
        const auto col = data_.get_column_by_index(*idx);
        if (col) {
            total += col->packed_size();
        }
    }
    return total;
}

auto MemoryStorage::alter(std::function<void(IStorage& storage)> modify) -> bool {
    std::lock_guard lock(mutex_);
    modify(*this);
    return true;
}

auto MemoryStorage::get_setting(std::string_view name)
    -> std::optional<common::SettingValueType> {
    (void)name;
    return std::nullopt;
}

auto MemoryStorage::lock() -> bool { return true; }
auto MemoryStorage::unlock() -> void {}

auto MemoryStorage::flush() -> bool { return true; }

void MemoryStorage::add_column_unlocked(std::string name, datatypes::DataTypePtr type) {
    column_names_.push_back(std::move(name));
    column_types_[column_names_.back()] = std::move(type);
}

void MemoryStorage::drop_column_unlocked(std::string name) {
    column_types_.erase(name);
    column_names_.erase(
        std::remove(column_names_.begin(), column_names_.end(), name),
        column_names_.end());
    if (data_.column_count() > 0) {
        data_.erase_column(name);
    }
}

void MemoryStorage::modify_column_unlocked(std::string name, datatypes::DataTypePtr type) {
    if (column_types_.find(name) == column_types_.end()) {
        throw common::Exception{
            "Unknown column: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
    }
    column_types_[name] = std::move(type);
}

void MemoryStorage::truncate_unlocked() {
    data_.reset();
    empty_ = true;
}

void MemoryStorage::set_columns_unlocked(
    std::unordered_map<std::string, datatypes::DataTypePtr> types) {
    column_types_ = std::move(types);
    column_names_.clear();
    for (const auto& [col_name, _] : column_types_) {
        column_names_.push_back(col_name);
    }
}

void MemoryStorage::add_column(std::string name, datatypes::DataTypePtr type) {
    std::lock_guard lock(mutex_);
    add_column_unlocked(std::move(name), std::move(type));
}

void MemoryStorage::drop_column(std::string name) {
    std::lock_guard lock(mutex_);
    drop_column_unlocked(std::move(name));
}

void MemoryStorage::modify_column(std::string name, datatypes::DataTypePtr type) {
    std::lock_guard lock(mutex_);
    modify_column_unlocked(std::move(name), std::move(type));
}

void MemoryStorage::truncate() {
    std::lock_guard lock(mutex_);
    truncate_unlocked();
}

void MemoryStorage::set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) {
    std::lock_guard lock(mutex_);
    set_columns_unlocked(std::move(types));
}

void MemoryStorage::load_block(const core::Block& block) {
    std::lock_guard lock(mutex_);

    column_names_.clear();
    column_types_.clear();
    data_.reset();

    for (const auto& col_name : block.column_names()) {
        const auto col = block.get_column_by_name(col_name);
        if (!col) {
            continue;
        }
        add_column_unlocked(col_name, col->get_data_type());
    }

    data_ = block.clone();
    empty_ = data_.row_count() == 0;
}

} // namespace mnemo::storages
