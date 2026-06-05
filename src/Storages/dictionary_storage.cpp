// src/Storages/dictionary_storage.cpp — Dictionary storage engine implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Storages/dictionary_storage.h"
#include "Core/block.h"
#include "Core/field.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::storages {

auto DictionaryStorage::create(std::string name) -> std::shared_ptr<DictionaryStorage> {
    auto storage = std::shared_ptr<DictionaryStorage>(new DictionaryStorage());
    storage->name_ = std::move(name);
    return storage;
}

DictionaryStorage::DictionaryStorage() = default;

auto DictionaryStorage::name() const -> std::string { return name_; }
auto DictionaryStorage::engine() const -> std::string { return "Dictionary"; }
auto DictionaryStorage::path() const -> std::string { return ""; }
auto DictionaryStorage::is_temporary() const -> bool { return false; }

auto DictionaryStorage::columns() const -> std::vector<std::string> {
    return {"key", "value"};
}

auto DictionaryStorage::column_types() const -> std::unordered_map<std::string, datatypes::DataTypePtr> {
    return column_types_;
}

auto DictionaryStorage::read(const std::vector<std::string>& column_names,
                             size_t max_block_size) -> core::Block {
    core::Block result;

    for (auto& col_name : column_names) {
        auto it = column_types_.find(col_name);
        if (it == column_types_.end()) continue;

        auto col = std::make_shared<columns::ColumnVector<double>>();
        for (auto& [key, val] : data_) {
            if (col_name == "key") {
                // Use hash of key as double value for column
                std::hash<std::string> hasher;
                (void)col->insert_at(col->size(), core::Field{static_cast<double>(hasher(key))});
            } else if (col_name == "value") {
                double d = 0.0;
                if (auto fv = val.as_float64()) {
                    d = *fv;
                } else if (auto iv = val.as_int64()) {
                    d = static_cast<double>(*iv);
                }
                (void)col->insert_at(col->size(), core::Field{d});
            }
        }
        result.add_column(col_name, col);
    }
    return result;
}

auto DictionaryStorage::write(const core::Block& block) -> bool {
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

        for (size_t r = 0; r < col->size(); ++r) {
            auto val = col->get_at(r);
            // Store in dictionary
            data_[col_name + "_" + std::to_string(r)] = val;
        }
    }
    empty_ = false;
    return true;
}

auto DictionaryStorage::empty() const -> bool { return data_.empty(); }

auto DictionaryStorage::row_count() const -> size_t {
    return data_.size();
}

auto DictionaryStorage::byte_count() const -> size_t {
    size_t total = 0;
    for (auto& [k, v] : data_) {
        total += k.size();
        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return;
            } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
                total += sizeof(T);
            } else if constexpr (std::is_same_v<T, std::string>) {
                total += val.size();
            }
        }, v.variant());
    }
    return total;
}

auto DictionaryStorage::alter(std::function<void(IStorage& storage)> modify) -> bool {
    modify(*this);
    return true;
}

auto DictionaryStorage::get_setting(std::string_view name)
    -> std::optional<common::SettingValueType> {
    return std::nullopt;
}

auto DictionaryStorage::lock() -> bool { return true; }
auto DictionaryStorage::unlock() -> void {}
auto DictionaryStorage::flush() -> bool { return true; }

auto DictionaryStorage::put(std::string key, core::Field value) -> bool {
    data_[std::move(key)] = std::move(value);
    return true;
}

auto DictionaryStorage::get(std::string key) -> std::optional<core::Field> {
    auto it = data_.find(std::move(key));
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

auto DictionaryStorage::contains(std::string key) -> bool {
    return data_.find(key) != data_.end();
}

auto DictionaryStorage::remove(std::string key) -> bool {
    return data_.erase(key) > 0;
}

auto DictionaryStorage::clear() -> void {
    data_.clear();
    empty_ = true;
}

auto DictionaryStorage::add_column(std::string name, datatypes::DataTypePtr type) -> void {
    column_names_.push_back(std::move(name));
    column_types_[column_names_.back()] = std::move(type);
}

auto DictionaryStorage::set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void {
    column_types_ = std::move(types);
    column_names_.clear();
    for (auto& [name, _] : column_types_) {
        column_names_.push_back(name);
    }
}

} // namespace mnemo::storages
