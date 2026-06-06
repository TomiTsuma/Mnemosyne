// src/ReplicaGroups/replica_group_manager.cpp

#include "ReplicaGroups/replica_group_manager.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <chrono>

namespace mnemo::replica_groups {

namespace {

auto now() -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::now();
}

} // namespace

auto ReplicaGroupManager::instance() -> ReplicaGroupManager& {
    static ReplicaGroupManager inst;
    return inst;
}

auto ReplicaGroupManager::make_replica_id(std::string_view group_name, uint32_t index) const
    -> std::string {
    return std::string{group_name} + "/r" + std::to_string(index);
}

auto ReplicaGroupManager::pick_online_nodes_unlocked(size_t count) const -> std::vector<std::string> {
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

auto ReplicaGroupManager::member_node_online_unlocked(const ReplicaMember& member) const -> bool {
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

auto ReplicaGroupManager::resize_members_unlocked(const ReplicaGroupEntry& group) -> void {
    auto& slots = members_[group.name];
    const auto factor = group.replication_factor;
    if (slots.size() < factor) {
        for (uint32_t i = static_cast<uint32_t>(slots.size()); i < factor; ++i) {
            ReplicaMember member;
            member.replica_id = make_replica_id(group.name, i);
            member.group_name = group.name;
            member.state = ReplicaState::Creating;
            member.role = (i == 0) ? ReplicaRole::Primary : ReplicaRole::Replica;
            slots.push_back(std::move(member));
        }
    } else if (slots.size() > factor) {
        slots.resize(factor);
        if (!slots.empty()) {
            slots[0].role = ReplicaRole::Primary;
            for (size_t i = 1; i < slots.size(); ++i) {
                slots[i].role = ReplicaRole::Replica;
            }
        }
    }
}

auto ReplicaGroupManager::create_group(ReplicaGroupEntry entry, bool if_not_exists) -> void {
    validate_replica_group_entry(entry);
    std::lock_guard lock{mutex_};
    if (groups_.contains(entry.name)) {
        if (if_not_exists) {
            return;
        }
        throw common::Exception{
            "Replica group already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    const auto ts = now();
    entry.created_at = ts;
    entry.updated_at = ts;
    const auto name = entry.name;
    groups_[name] = std::move(entry);
    resize_members_unlocked(groups_[name]);
    place_replicas_unlocked(name);
}

auto ReplicaGroupManager::alter_group(std::string_view name, std::optional<uint32_t> replicas,
                                      std::optional<ConsistencyMode> consistency) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        throw common::Exception{
            "Unknown replica group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (replicas) {
        if (*replicas < 1) {
            throw common::Exception{
                "REPLICAS must be at least 1",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        it->second.replication_factor = *replicas;
        resize_members_unlocked(it->second);
    }
    if (consistency) {
        it->second.consistency_mode = *consistency;
    }
    it->second.updated_at = now();
    place_replicas_unlocked(it->second.name);
}

auto ReplicaGroupManager::drop_group(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        if (if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown replica group: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (it->second.reference_count > 0) {
        throw common::Exception{
            "Replica group is in use: " + name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    groups_.erase(it);
    members_.erase(name);
}

auto ReplicaGroupManager::acquire_group(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        throw common::Exception{
            "Unknown replica group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    ++it->second.reference_count;
}

auto ReplicaGroupManager::release_group(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    if (it == groups_.end()) {
        return;
    }
    if (it->second.reference_count > 0) {
        --it->second.reference_count;
    }
}

auto ReplicaGroupManager::get_group(std::string_view name) -> ReplicaGroupEntry* {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    return it != groups_.end() ? &it->second : nullptr;
}

auto ReplicaGroupManager::get_group(std::string_view name) const -> const ReplicaGroupEntry* {
    std::lock_guard lock{mutex_};
    auto it = groups_.find(std::string{name});
    return it != groups_.end() ? &it->second : nullptr;
}

auto ReplicaGroupManager::has_group(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return groups_.contains(std::string{name});
}

auto ReplicaGroupManager::list_groups() const -> std::vector<ReplicaGroupEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ReplicaGroupEntry> result;
    result.reserve(groups_.size());
    for (const auto& [_, g] : groups_) {
        result.push_back(g);
    }
    std::sort(result.begin(), result.end(),
              [](const ReplicaGroupEntry& a, const ReplicaGroupEntry& b) {
                  return a.name < b.name;
              });
    return result;
}

auto ReplicaGroupManager::list_members(std::string_view group_name) const
    -> std::vector<ReplicaMember> {
    std::lock_guard lock{mutex_};
    auto it = members_.find(std::string{group_name});
    if (it == members_.end()) {
        return {};
    }
    return it->second;
}

auto ReplicaGroupManager::list_all_members() const -> std::vector<ReplicaMember> {
    std::lock_guard lock{mutex_};
    std::vector<ReplicaMember> result;
    for (const auto& [_, members] : members_) {
        for (const auto& m : members) {
            result.push_back(m);
        }
    }
    return result;
}

auto ReplicaGroupManager::list_failover_events() const -> std::vector<FailoverEvent> {
    std::lock_guard lock{mutex_};
    return failover_events_;
}

auto ReplicaGroupManager::place_replicas(std::string_view group_name) -> void {
    std::lock_guard lock{mutex_};
    place_replicas_unlocked(group_name);
}

auto ReplicaGroupManager::place_replicas_unlocked(std::string_view group_name) -> void {
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
        member.role = (i == 0) ? ReplicaRole::Primary : ReplicaRole::Replica;
        if (i < nodes.size()) {
            member.node_id = nodes[i];
            member.state = ReplicaState::Syncing;
            mgr.assign_replica(member.node_id, member.replica_id);
            if (member_node_online_unlocked(member)) {
                member.state = ReplicaState::Online;
                member.last_sync_at = now();
            }
        } else {
            member.node_id.clear();
            member.state = ReplicaState::Offline;
        }
    }

    bool any_online = false;
    bool all_online = !mit->second.empty();
    for (const auto& m : mit->second) {
        if (m.state == ReplicaState::Online) {
            any_online = true;
        } else {
            all_online = false;
        }
    }
    if (all_online) {
        git->second.status = ReplicaGroupStatus::Online;
    } else if (any_online) {
        git->second.status = ReplicaGroupStatus::Degraded;
    } else {
        git->second.status = ReplicaGroupStatus::Offline;
    }
    git->second.updated_at = now();
}

auto ReplicaGroupManager::on_node_offline(std::string_view node_id) -> void {
    std::lock_guard lock{mutex_};
    for (auto& [group_name, slots] : members_) {
        bool primary_offline = false;
        for (auto& member : slots) {
            if (member.node_id == node_id) {
                member.state = ReplicaState::Offline;
                if (member.role == ReplicaRole::Primary) {
                    primary_offline = true;
                }
            }
        }
        if (!primary_offline) {
            continue;
        }

        ReplicaMember* candidate = nullptr;
        for (auto& member : slots) {
            if (member.role != ReplicaRole::Primary &&
                member_node_online_unlocked(member)) {
                candidate = &member;
                break;
            }
        }
        if (!candidate) {
            if (auto git = groups_.find(group_name); git != groups_.end()) {
                git->second.status = ReplicaGroupStatus::Offline;
            }
            continue;
        }

        for (auto& member : slots) {
            if (member.role == ReplicaRole::Primary) {
                member.role = ReplicaRole::Replica;
            }
        }
        const std::string old_primary{node_id};
        candidate->state = ReplicaState::Promoting;
        candidate->role = ReplicaRole::Primary;
        candidate->failover_count += 1;
        candidate->state = ReplicaState::Online;
        candidate->last_sync_at = now();

        FailoverEvent event;
        event.group_name = group_name;
        event.old_primary_node = old_primary;
        event.new_primary_node = candidate->node_id;
        event.occurred_at = now();
        failover_events_.push_back(std::move(event));

        if (auto git = groups_.find(group_name); git != groups_.end()) {
            git->second.status = ReplicaGroupStatus::Degraded;
            git->second.updated_at = now();
        }
    }
}

auto ReplicaGroupManager::on_node_online(std::string_view node_id) -> void {
    std::lock_guard lock{mutex_};
    for (auto& [group_name, slots] : members_) {
        for (auto& member : slots) {
            if (member.node_id != node_id) {
                continue;
            }
            member.state = ReplicaState::Recovering;
            member.state = ReplicaState::Syncing;
            member.state = ReplicaState::Online;
            member.last_sync_at = now();
        }
        if (auto git = groups_.find(group_name); git != groups_.end()) {
            git->second.status = ReplicaGroupStatus::Online;
            git->second.updated_at = now();
        }
    }
}

auto ReplicaGroupManager::refresh_lag() -> void {
    std::lock_guard lock{mutex_};
    const auto& node_mgr = nodes::NodeManager::instance();
    const auto tick = now();
    for (auto& [_, slots] : members_) {
        for (auto& member : slots) {
            if (member.node_id.empty()) {
                member.lag_ms = 0;
                continue;
            }
            const auto* node = node_mgr.get_node(member.node_id);
            if (!node) {
                member.lag_ms = 0;
                member.state = ReplicaState::Offline;
                continue;
            }
            if (node->status == nodes::NodeStatus::Offline) {
                member.state = ReplicaState::Offline;
                member.lag_ms = 0;
                continue;
            }
            const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                tick - node->last_heartbeat_at);
            member.lag_ms = static_cast<uint64_t>(std::max<int64_t>(0, age.count()));
            if (member.state != ReplicaState::Promoting) {
                member.state = ReplicaState::Online;
            }
        }
    }
}

auto ReplicaGroupManager::sweep_node_status() -> void {
    const auto entries = nodes::NodeManager::instance().list_entries();
    std::vector<std::string> went_offline;
    std::vector<std::string> came_online;

    {
        std::lock_guard lock{mutex_};
        for (const auto& entry : entries) {
            const auto status = nodes::node_status_name(entry.status);
            const auto it = last_node_status_.find(entry.node_id);
            if (it == last_node_status_.end()) {
                last_node_status_[entry.node_id] = status;
                continue;
            }
            const auto& prev = it->second;
            if (prev != "OFFLINE" && status == "OFFLINE") {
                went_offline.push_back(entry.node_id);
            } else if (prev == "OFFLINE" && status == "ONLINE") {
                came_online.push_back(entry.node_id);
            }
            it->second = status;
        }
    }

    refresh_lag();
    for (const auto& id : went_offline) {
        on_node_offline(id);
    }
    for (const auto& id : came_online) {
        on_node_online(id);
    }
}

} // namespace mnemo::replica_groups
