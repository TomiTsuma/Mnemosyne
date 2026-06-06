// src/ShardGroups/shard_group_manager.h — Cluster-wide shard group registry

#pragma once

#include "ShardGroups/shard_group_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::shard_groups {

class ShardGroupManager {
public:
    static auto instance() -> ShardGroupManager&;

    auto create_group(ShardGroupEntry entry, bool if_not_exists) -> void;
    auto alter_group(std::string_view name, std::optional<uint32_t> shards) -> void;
    auto drop_group(std::string name, bool if_exists) -> void;

    auto acquire_group(std::string_view name) -> void;
    auto release_group(std::string_view name) -> void;

    auto get_group(std::string_view name) -> ShardGroupEntry*;
    auto get_group(std::string_view name) const -> const ShardGroupEntry*;
    [[nodiscard]] auto has_group(std::string_view name) const -> bool;
    [[nodiscard]] auto list_groups() const -> std::vector<ShardGroupEntry>;
    [[nodiscard]] auto list_members(std::string_view group_name) const
        -> std::vector<ShardMember>;
    [[nodiscard]] auto list_all_members() const -> std::vector<ShardMember>;

    auto place_shards(std::string_view group_name) -> void;
    auto place_shards_unlocked(std::string_view group_name) -> void;

private:
    ShardGroupManager() = default;

    auto make_shard_id(std::string_view group_name, uint32_t index) const -> std::string;
    auto resize_members_unlocked(const ShardGroupEntry& group) -> void;
    auto pick_online_nodes_unlocked(size_t count) const -> std::vector<std::string>;
    auto member_node_online_unlocked(const ShardMember& member) const -> bool;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ShardGroupEntry> groups_;
    std::unordered_map<std::string, std::vector<ShardMember>> members_;
};

} // namespace mnemo::shard_groups
