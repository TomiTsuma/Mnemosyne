// src/ReplicaGroups/replica_group_catalog.h — REPLICA_GROUP catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::replica_groups {

enum class ReplicaStrategy { PrimaryReplica, MultiPrimary, Observer };

enum class ConsistencyMode { Synchronous, Asynchronous, Quorum };

enum class PlacementPolicy { NodeAware, RackAware, RegionAware };

enum class ReplicaState {
    Creating,
    Syncing,
    Online,
    Degraded,
    Offline,
    Promoting,
    Recovering
};

enum class ReplicaRole { Primary, Replica, Observer };

enum class ReplicaGroupStatus { Online, Degraded, Offline };

[[nodiscard]] auto replica_strategy_name(ReplicaStrategy strategy) -> std::string;
[[nodiscard]] auto consistency_mode_name(ConsistencyMode mode) -> std::string;
[[nodiscard]] auto placement_policy_name(PlacementPolicy policy) -> std::string;
[[nodiscard]] auto replica_state_name(ReplicaState state) -> std::string;
[[nodiscard]] auto replica_role_name(ReplicaRole role) -> std::string;
[[nodiscard]] auto replica_group_status_name(ReplicaGroupStatus status) -> std::string;

[[nodiscard]] auto parse_consistency_mode(std::string_view name) -> ConsistencyMode;
[[nodiscard]] auto parse_replica_strategy(std::string_view name) -> ReplicaStrategy;
[[nodiscard]] auto parse_placement_policy(std::string_view name) -> PlacementPolicy;

struct ReplicaGroupEntry {
    std::string name;
    uint32_t replication_factor = 1;
    ReplicaStrategy strategy = ReplicaStrategy::PrimaryReplica;
    ConsistencyMode consistency_mode = ConsistencyMode::Quorum;
    PlacementPolicy placement_policy = PlacementPolicy::NodeAware;
    ReplicaGroupStatus status = ReplicaGroupStatus::Online;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    std::string owner;
    size_t reference_count = 0;
};

struct ReplicaMember {
    std::string replica_id;
    std::string group_name;
    std::string node_id;
    ReplicaRole role = ReplicaRole::Replica;
    ReplicaState state = ReplicaState::Creating;
    uint64_t lag_ms = 0;
    std::chrono::system_clock::time_point last_sync_at{};
    uint32_t failover_count = 0;
};

struct FailoverEvent {
    std::string group_name;
    std::string old_primary_node;
    std::string new_primary_node;
    std::chrono::system_clock::time_point occurred_at{};
};

void validate_replica_group_entry(const ReplicaGroupEntry& entry);

} // namespace mnemo::replica_groups
