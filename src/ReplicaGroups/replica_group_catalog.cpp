// src/ReplicaGroups/replica_group_catalog.cpp

#include "ReplicaGroups/replica_group_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::replica_groups {

namespace {

auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto replica_strategy_name(ReplicaStrategy strategy) -> std::string {
    switch (strategy) {
        case ReplicaStrategy::PrimaryReplica: return "PRIMARY_REPLICA";
        case ReplicaStrategy::MultiPrimary:   return "MULTI_PRIMARY";
        case ReplicaStrategy::Observer:       return "OBSERVER";
    }
    return "UNKNOWN";
}

auto consistency_mode_name(ConsistencyMode mode) -> std::string {
    switch (mode) {
        case ConsistencyMode::Synchronous:  return "SYNCHRONOUS";
        case ConsistencyMode::Asynchronous: return "ASYNCHRONOUS";
        case ConsistencyMode::Quorum:       return "QUORUM";
    }
    return "UNKNOWN";
}

auto placement_policy_name(PlacementPolicy policy) -> std::string {
    switch (policy) {
        case PlacementPolicy::NodeAware:   return "NODE_AWARE";
        case PlacementPolicy::RackAware:   return "RACK_AWARE";
        case PlacementPolicy::RegionAware: return "REGION_AWARE";
    }
    return "UNKNOWN";
}

auto replica_state_name(ReplicaState state) -> std::string {
    switch (state) {
        case ReplicaState::Creating:   return "CREATING";
        case ReplicaState::Syncing:    return "SYNCING";
        case ReplicaState::Online:     return "ONLINE";
        case ReplicaState::Degraded:   return "DEGRADED";
        case ReplicaState::Offline:    return "OFFLINE";
        case ReplicaState::Promoting:  return "PROMOTING";
        case ReplicaState::Recovering: return "RECOVERING";
    }
    return "UNKNOWN";
}

auto replica_role_name(ReplicaRole role) -> std::string {
    switch (role) {
        case ReplicaRole::Primary:  return "PRIMARY";
        case ReplicaRole::Replica:  return "REPLICA";
        case ReplicaRole::Observer: return "OBSERVER";
    }
    return "UNKNOWN";
}

auto replica_group_status_name(ReplicaGroupStatus status) -> std::string {
    switch (status) {
        case ReplicaGroupStatus::Online:   return "ONLINE";
        case ReplicaGroupStatus::Degraded: return "DEGRADED";
        case ReplicaGroupStatus::Offline:  return "OFFLINE";
    }
    return "UNKNOWN";
}

auto parse_consistency_mode(std::string_view name) -> ConsistencyMode {
    const auto upper = to_upper(std::string{name});
    if (upper == "SYNCHRONOUS")  return ConsistencyMode::Synchronous;
    if (upper == "ASYNCHRONOUS") return ConsistencyMode::Asynchronous;
    if (upper == "QUORUM")       return ConsistencyMode::Quorum;
    throw common::Exception{
        "Unknown consistency mode: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_replica_strategy(std::string_view name) -> ReplicaStrategy {
    const auto upper = to_upper(std::string{name});
    if (upper == "PRIMARY_REPLICA" || upper == "PRIMARY") return ReplicaStrategy::PrimaryReplica;
    if (upper == "MULTI_PRIMARY") return ReplicaStrategy::MultiPrimary;
    if (upper == "OBSERVER")      return ReplicaStrategy::Observer;
    throw common::Exception{
        "Unknown replica strategy: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_placement_policy(std::string_view name) -> PlacementPolicy {
    const auto upper = to_upper(std::string{name});
    if (upper == "NODE_AWARE")   return PlacementPolicy::NodeAware;
    if (upper == "RACK_AWARE")   return PlacementPolicy::RackAware;
    if (upper == "REGION_AWARE") return PlacementPolicy::RegionAware;
    throw common::Exception{
        "Unknown placement policy: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

void validate_replica_group_entry(const ReplicaGroupEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Replica group name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.replication_factor < 1) {
        throw common::Exception{
            "REPLICAS must be at least 1",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

} // namespace mnemo::replica_groups
