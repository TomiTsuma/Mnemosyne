// src/ShardGroups/shard_group_manager.cpp

#include "ShardGroups/shard_group_manager.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <chrono>

namespace mnemo::shard_groups {

namespace {

auto now() -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::now();
}

} // namespace

auto ShardGroupManager::instance() -> ShardGroupManager& {
    static ShardGroupManager inst;
    return inst;
}

auto ShardGroupManager::make_shard_id(std::string_view group_name, uint32_t index) const
    -> std::string {
    return std::string{group_name} + "/s" + std::to_string(index);
}

auto ShardGroupManager::pick_online_nodes_unlocked(size_t count) const -> std::vector<std::string> {
    const auto entries = nodes::NodeManager::instance().list_entries();
    std::vector<const nodes::NodeEntry*> candidates;
    candidates.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.status != nodes::NodeStatus::Online &&
            entry.status != nodes::NodeStatus::Busy) {
            continue;
        }
        candidates.push_back(&entry);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const nodes::NodeEntry* a, const nodes::NodeEntry* b) {
                  if (a->is_self != b->is_self) {
                      return !a->is_self;
                  }
                  return a->node_id < b->node_id;
              });
    std::vector<std::string> result;
    result.reserve(count);
    for (const auto* entry : candidates) {
        result.push_back(entry->node_id);
        if (result.size() >= count) {
            break;
        }
    }
    return result;
}

auto ShardGroupManager::member_node_online_unlocked(const ShardMember& member) const -> bool {
    if (member.node_id.empty()) {
        return false;
    }
    const auto* node = nodes::NodeManager::instance().get_node(member.node_id);
    if (!node) {
        return false;
    }
    return node->status == nodes::NodeStatus::Online ||
           node->status == nodes::NodeStatus::Busy;
}

auto ShardGroupManager::resize_members_unlocked(const ShardGroupEntry& group) -> void {
    auto& slots = members_[group.name];
    const auto count = group.shard_count;
    if (slots.size() < count) {
        for (uint32_t i = static_cast<uint32_t>(slots.size()); i < count; ++i) {
            ShardMember member;
            member.shard_id = make_shard_id(group.name, i);
            member.group_name = group.name;
            member.state = ShardState::Creating;
            slots.push_back(std::move(member));
        }
    } else if (slots.size() > count) {
        slots.resize(count);
    }
}

auto ShardGroupManager::create_group(ShardGroupEntry entry, bool if_not_exists) -> void {
    validate_shard_group_entry(entry);
    std::lock_guard lock{mutex_};
    if (groups_.contains(entry.name)) {
        if (if_not_exists) {
            return;
        }
        throw common::Exception{
            "Shard group already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto ts = now();
    entry.created_at = ts;
    entry.updated_at = ts;
    const auto name = entry.name;
    groups_[name] = std::move(entry);
    resize_members_unlocked(groups_[name]);
    place_shards_unlocked(name);
}

auto ShardGroupManager::alter_group(std::string_view name, std::optional<uint32_t> shards)
    -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        throw common::Exception{
            "Unknown shard group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (shards) {
        if (*shards < 1) {
            throw common::Exception{
                "SHARDS must be at least 1",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        it->second.shard_count = *shards;
        resize_members_unlocked(it->second);
    }
    it->second.updated_at = now();
    place_shards_unlocked(it->second.name);
}

auto ShardGroupManager::drop_group(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        if (if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown shard group: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (it->second.reference_count > 0) {
        throw common::Exception{
            "Shard group is in use: " + name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    groups_.erase(it);
    members_.erase(name);
}

auto ShardGroupManager::acquire_group(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        throw common::Exception{
            "Unknown shard group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    ++it->second.reference_count;
}

auto ShardGroupManager::release_group(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        return;
    }
    if (it->second.reference_count > 0) {
        --it->second.reference_count;
    }
}

auto ShardGroupManager::get_group(std::string_view name) -> ShardGroupEntry* {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    return it != groups_.end() ? &it->second : nullptr;
}

auto ShardGroupManager::get_group(std::string_view name) const -> const ShardGroupEntry* {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    return it != groups_.end() ? &it->second : nullptr;
}

auto ShardGroupManager::has_group(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return groups_.contains(std::string{name});
}

auto ShardGroupManager::list_groups() const -> std::vector<ShardGroupEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ShardGroupEntry> result;
    result.reserve(groups_.size());
    for (const auto& [_, g] : groups_) {
        result.push_back(g);
    }
    std::sort(result.begin(), result.end(),
              [](const ShardGroupEntry& a, const ShardGroupEntry& b) {
                  return a.name < b.name;
              });
    return result;
}

auto ShardGroupManager::list_members(std::string_view group_name) const
    -> std::vector<ShardMember> {
    std::lock_guard lock{mutex_};
    auto it = members_.find(std::string{group_name});
    if (it == members_.end()) {
        return {};
    }
    return it->second;
}

auto ShardGroupManager::list_all_members() const -> std::vector<ShardMember> {
    std::lock_guard lock{mutex_};
    std::vector<ShardMember> result;
    for (const auto& [_, members] : members_) {
        for (const auto& m : members) {
            result.push_back(m);
        }
    }
    return result;
}

auto ShardGroupManager::place_shards(std::string_view group_name) -> void {
    std::lock_guard lock{mutex_};
    place_shards_unlocked(group_name);
}

auto ShardGroupManager::place_shards_unlocked(std::string_view group_name) -> void {
    auto git = groups_.find(std::string{group_name});
    if (git == groups_.end()) {
        return;
    }
    auto mit = members_.find(std::string{group_name});
    if (mit == members_.end()) {
        return;
    }

    const auto nodes = pick_online_nodes_unlocked(mit->second.size());
    auto& mgr = nodes::NodeManager::instance();

    for (size_t i = 0; i < mit->second.size(); ++i) {
        auto& member = mit->second[i];
        if (i < nodes.size()) {
            member.node_id = nodes[i % nodes.size()];
            member.state = ShardState::Creating;
            mgr.assign_partition(member.node_id, member.shard_id);
            if (member_node_online_unlocked(member)) {
                member.state = ShardState::Active;
            }
        } else {
            member.node_id.clear();
            member.state = ShardState::Offline;
        }
    }

    bool any_active = false;
    bool all_active = !mit->second.empty();
    for (const auto& m : mit->second) {
        if (m.state == ShardState::Active) {
            any_active = true;
        } else {
            all_active = false;
        }
    }
    if (all_active) {
        git->second.status = ShardGroupStatus::Online;
    } else if (any_active) {
        git->second.status = ShardGroupStatus::Degraded;
    } else {
        git->second.status = ShardGroupStatus::Offline;
    }
    git->second.updated_at = now();
}

} // namespace mnemo::shard_groups
