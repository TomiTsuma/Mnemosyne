// src/ReplicaGroups/replica_group_manager.h — Cluster-wide replica group registry

#pragma once

#include "ReplicaGroups/replica_group_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::replica_groups {

class ReplicaGroupManager {
public:
    static auto instance() -> ReplicaGroupManager&;

    auto create_group(ReplicaGroupEntry entry, bool if_not_exists) -> void;
    auto alter_group(std::string_view name, std::optional<uint32_t> replicas,
                     std::optional<ConsistencyMode> consistency) -> void;
    auto drop_group(std::string name, bool if_exists) -> void;

    auto acquire_group(std::string_view name) -> void;
    auto release_group(std::string_view name) -> void;

    auto get_group(std::string_view name) -> ReplicaGroupEntry*;
    auto get_group(std::string_view name) const -> const ReplicaGroupEntry*;
    [[nodiscard]] auto has_group(std::string_view name) const -> bool;
    [[nodiscard]] auto list_groups() const -> std::vector<ReplicaGroupEntry>;
    [[nodiscard]] auto list_members(std::string_view group_name) const
        -> std::vector<ReplicaMember>;
    [[nodiscard]] auto list_all_members() const -> std::vector<ReplicaMember>;
    [[nodiscard]] auto list_failover_events() const -> std::vector<FailoverEvent>;

    auto place_replicas(std::string_view group_name) -> void;
    auto place_replicas_unlocked(std::string_view group_name) -> void;
    auto on_node_offline(std::string_view node_id) -> void;
    auto on_node_online(std::string_view node_id) -> void;
    auto refresh_lag() -> void;
    auto sweep_node_status() -> void;

private:
    ReplicaGroupManager() = default;

    auto make_replica_id(std::string_view group_name, uint32_t index) const -> std::string;
    auto resize_members_unlocked(const ReplicaGroupEntry& group) -> void;
    auto pick_online_nodes_unlocked(size_t count) const -> std::vector<std::string>;
    auto member_node_online_unlocked(const ReplicaMember& member) const -> bool;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ReplicaGroupEntry> groups_;
    std::unordered_map<std::string, std::vector<ReplicaMember>> members_;
    std::vector<FailoverEvent> failover_events_;
    std::unordered_map<std::string, std::string> last_node_status_;
};

} // namespace mnemo::replica_groups
