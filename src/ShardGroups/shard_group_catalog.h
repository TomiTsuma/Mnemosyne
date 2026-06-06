// src/ShardGroups/shard_group_catalog.h — SHARD_GROUP catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace mnemo::shard_groups {

enum class ShardStrategy { Hash, Range, List, Composite };

enum class ShardState {
    Creating,
    Active,
    Rebalancing,
    Splitting,
    Merging,
    Degraded,
    Offline
};

enum class ShardGroupStatus { Online, Degraded, Offline };

[[nodiscard]] auto shard_strategy_name(ShardStrategy strategy) -> std::string;
[[nodiscard]] auto shard_state_name(ShardState state) -> std::string;
[[nodiscard]] auto shard_group_status_name(ShardGroupStatus status) -> std::string;

[[nodiscard]] auto parse_shard_strategy(std::string_view name) -> ShardStrategy;

struct ShardGroupEntry {
    std::string name;
    ShardStrategy strategy = ShardStrategy::Hash;
    uint32_t shard_count = 1;
    std::string shard_key;
    ShardGroupStatus status = ShardGroupStatus::Online;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    std::string owner;
    size_t reference_count = 0;
};

struct ShardMember {
    std::string shard_id;
    std::string group_name;
    std::string node_id;
    ShardState state = ShardState::Creating;
    uint64_t row_count = 0;
    uint64_t size_bytes = 0;
};

void validate_shard_group_entry(const ShardGroupEntry& entry);

} // namespace mnemo::shard_groups
