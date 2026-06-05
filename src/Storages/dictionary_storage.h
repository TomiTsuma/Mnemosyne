// src/Storages/dictionary_storage.h — Dictionary storage engine
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_storage.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include "Core/field.h"

namespace mnemo::storages {

// ── DictionaryStorage — key-value dictionary ──
class DictionaryStorage final : public IStorage {
public:
    static auto create(std::string name) -> std::shared_ptr<DictionaryStorage>;

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

    // Dictionary-specific
    auto put(std::string key, core::Field value) -> bool;
    auto get(std::string key) -> std::optional<core::Field>;
    auto contains(std::string key) -> bool;
    auto remove(std::string key) -> bool;
    auto clear() -> void;
    auto add_column(std::string name, datatypes::DataTypePtr type) -> void;
    auto set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void;

private:
    DictionaryStorage();
    std::string               name_;
    std::unordered_map<std::string, core::Field> data_;
    std::unordered_map<std::string, datatypes::DataTypePtr> column_types_;
    std::vector<std::string> column_names_;
    bool empty_ = true;
};

} // namespace mnemo::storages
