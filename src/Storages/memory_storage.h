// src/Storages/memory_storage.h — In-memory storage engine
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_storage.h"
#include "Core/block.h"
#include "Common/settings.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mnemo::storages {

// ── MemoryStorage — temporary in-memory table ──
class MemoryStorage final : public IStorage {
public:
    static auto create(std::string name) -> std::shared_ptr<MemoryStorage>;

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

    // Memory-specific
    auto add_column(std::string name, datatypes::DataTypePtr type) -> void;
    auto set_columns(std::unordered_map<std::string, datatypes::DataTypePtr> types) -> void;
    void load_block(const core::Block& block);

private:
    MemoryStorage();

    void add_column_unlocked(std::string name, datatypes::DataTypePtr type);
    void set_columns_unlocked(std::unordered_map<std::string, datatypes::DataTypePtr> types);

    std::string              name_;
    std::unordered_map<std::string, datatypes::DataTypePtr> column_types_;
    std::vector<std::string> column_names_;
    core::Block              data_;
    bool                     empty_ = true;
    mutable std::mutex       mutex_;
};

} // namespace mnemo::storages
