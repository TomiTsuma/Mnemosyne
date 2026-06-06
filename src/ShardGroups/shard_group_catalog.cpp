// src/ShardGroups/shard_group_catalog.cpp

#include "ShardGroups/shard_group_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>

namespace mnemo::shard_groups {

namespace {

auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto shard_strategy_name(ShardStrategy strategy) -> std::string {
    switch (strategy) {
        case ShardStrategy::Hash:      return "HASH";
        case ShardStrategy::Range:     return "RANGE";
        case ShardStrategy::List:      return "LIST";
        case ShardStrategy::Composite: return "COMPOSITE";
    }
    return "UNKNOWN";
}

auto shard_state_name(ShardState state) -> std::string {
    switch (state) {
        case ShardState::Creating:    return "CREATING";
        case ShardState::Active:      return "ACTIVE";
        case ShardState::Rebalancing: return "REBALANCING";
        case ShardState::Splitting:   return "SPLITTING";
        case ShardState::Merging:     return "MERGING";
        case ShardState::Degraded:    return "DEGRADED";
        case ShardState::Offline:     return "OFFLINE";
    }
    return "UNKNOWN";
}

auto shard_group_status_name(ShardGroupStatus status) -> std::string {
    switch (status) {
        case ShardGroupStatus::Online:   return "ONLINE";
        case ShardGroupStatus::Degraded: return "DEGRADED";
        case ShardGroupStatus::Offline:  return "OFFLINE";
    }
    return "UNKNOWN";
}

auto parse_shard_strategy(std::string_view name) -> ShardStrategy {
    const auto upper = to_upper(std::string{name});
    if (upper == "HASH")      return ShardStrategy::Hash;
    if (upper == "RANGE")     return ShardStrategy::Range;
    if (upper == "LIST")      return ShardStrategy::List;
    if (upper == "COMPOSITE") return ShardStrategy::Composite;
    throw common::Exception{
        "Unknown shard strategy: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

void validate_shard_group_entry(const ShardGroupEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Shard group name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.shard_count < 1) {
        throw common::Exception{
            "SHARDS must be at least 1",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.strategy == ShardStrategy::Hash && entry.shard_key.empty()) {
        throw common::Exception{
            "HASH sharding requires a KEY column",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

} // namespace mnemo::shard_groups
