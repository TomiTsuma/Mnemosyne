// src/Storages/i_storage.h — IStorage: abstract column storage interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Common/types.h"
#include "Core/block.h"
#include "DataTypes/data_type.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include "Common/settings.h"

namespace mnemo::storages {

// ── IStorage — abstract storage interface ──
// All concrete storages (File, Memory, Dictionary, etc.) implement this.
class IStorage {
public:
    virtual ~IStorage() = default;

    // Storage identification
    [[nodiscard]] virtual auto name()      const -> std::string = 0;
    [[nodiscard]] virtual auto engine()    const -> std::string = 0;
    [[nodiscard]] virtual auto path()      const -> std::string = 0;
    [[nodiscard]] virtual auto is_temporary() const -> bool = 0;

    // Schema information
    [[nodiscard]] virtual auto columns()       const -> std::vector<std::string> = 0;
    [[nodiscard]] virtual auto column_types() const -> std::unordered_map<std::string, datatypes::DataTypePtr> = 0;

    // Block reading
    [[nodiscard]] virtual auto read(
        const std::vector<std::string>& column_names,
        size_t max_block_size = 0) -> core::Block = 0;

    // Block writing
    virtual auto write(const core::Block& block) -> bool = 0;

    // Mutation operations (ALTER TABLE, etc.)
    virtual auto alter(
        std::function<void(IStorage& storage)> modify) -> bool { (void)modify; return false; }

    // Check if storage is empty
    [[nodiscard]] virtual auto empty() const -> bool = 0;

    // Statistics
    [[nodiscard]] virtual auto row_count()  const -> size_t = 0;
    [[nodiscard]] virtual auto byte_count() const -> size_t = 0;

    // Get a named setting
    [[nodiscard]] virtual auto get_setting(std::string_view name)
        -> std::optional<common::SettingValueType> { (void)name; return std::nullopt; }

    // Lock/unlock for concurrent access
    virtual auto lock()  -> bool { return false; }
    virtual auto unlock() -> void {}

    // Flush — persist to disk
    virtual auto flush() -> bool { return false; }

    // Optional replica group attachment (REPLICA_GROUP first-class entity)
    [[nodiscard]] virtual auto replica_group_name() const -> std::string { return {}; }
    virtual auto set_replica_group_name(std::string name) -> void { (void)name; }

    // Optional shard group attachment (SHARD_GROUP first-class entity)
    [[nodiscard]] virtual auto shard_group_name() const -> std::string { return {}; }
    virtual auto set_shard_group_name(std::string name) -> void { (void)name; }
};

} // namespace mnemo::storages
