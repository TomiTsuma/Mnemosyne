// src/Nodes/node_manager.cpp — Node manager implementation

#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::nodes {

auto NodeManager::instance() -> NodeManager& {
    static NodeManager inst;
    return inst;
}

auto NodeManager::bootstrap_self(std::string_view node_id, std::string_view host,
                                 uint16_t port, std::string_view cluster_id) -> void {
    std::lock_guard lock{mutex_};
    self_node_id_ = std::string{node_id};
    if (nodes_.contains(self_node_id_)) {
        return;
    }
    NodeEntry entry;
    entry.node_id = self_node_id_;
    entry.node_name = self_node_id_;
    entry.cluster_id = std::string{cluster_id};
    entry.node_type = NodeType::Hybrid;
    entry.node_role = NodeRole::Coordinator;
    entry.status = NodeStatus::Online;
    entry.host = std::string{host};
    entry.port = port;
    entry.resources = detect_local_resources();
    entry.capabilities = capabilities_for_type(entry.node_type);
    entry.is_self = true;
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    entry.last_heartbeat_at = now;
    nodes_.emplace(entry.node_name, std::move(entry));
}

auto NodeManager::create_node(NodeEntry entry, bool if_not_exists) -> void {
    validate_node_entry(entry);
    std::lock_guard lock{mutex_};
    if (nodes_.contains(entry.node_name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Node already exists: " + entry.node_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    if (entry.capabilities.empty()) {
        entry.capabilities = capabilities_for_type(entry.node_type);
    }
    if (entry.resources.cpu_cores == 0) {
        entry.resources = detect_local_resources();
    }
    const auto now = std::chrono::system_clock::now();
    entry.created_at = now;
    entry.updated_at = now;
    entry.node_id = entry.node_name;
    if (entry.status == NodeStatus::Registering && entry.host.empty()) {
        entry.status = NodeStatus::Registering;
    } else if (!entry.host.empty()) {
        entry.status = NodeStatus::Online;
        entry.last_heartbeat_at = now;
    }
    nodes_.emplace(entry.node_name, std::move(entry));
}

auto NodeManager::register_node(std::string_view name, std::string_view host,
                                uint16_t port) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        NodeEntry entry;
        entry.node_id = std::string{name};
        entry.node_name = std::string{name};
        entry.host = std::string{host};
        entry.port = port;
        entry.node_type = NodeType::Hybrid;
        entry.node_role = NodeRole::Worker;
        entry.capabilities = capabilities_for_type(entry.node_type);
        entry.resources = detect_local_resources();
        const auto now = std::chrono::system_clock::now();
        entry.created_at = now;
        entry.updated_at = now;
        entry.last_heartbeat_at = now;
        entry.status = NodeStatus::Online;
        nodes_.emplace(entry.node_name, std::move(entry));
        return;
    }
    it->second.host = std::string{host};
    it->second.port = port;
    it->second.status = NodeStatus::Online;
    const auto now = std::chrono::system_clock::now();
    it->second.updated_at = now;
    it->second.last_heartbeat_at = now;
}

auto NodeManager::alter_node(std::string_view name, std::optional<NodeRole> role,
                             std::optional<NodeType> type, std::optional<NodeStatus> status,
                             std::optional<std::string_view> cluster_id) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (role) {
        it->second.node_role = *role;
    }
    if (type) {
        it->second.node_type = *type;
        it->second.capabilities = capabilities_for_type(*type);
    }
    if (status) {
        it->second.status = *status;
    }
    if (cluster_id) {
        it->second.cluster_id = std::string{*cluster_id};
    }
    it->second.updated_at = std::chrono::system_clock::now();
}

auto NodeManager::drain_node(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (it->second.is_self) {
        throw common::Exception{
            "Cannot drain self node: " + std::string{name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    it->second.status = NodeStatus::Draining;
    it->second.updated_at = std::chrono::system_clock::now();
}

auto NodeManager::remove_node(std::string_view name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (it->second.is_self) {
        throw common::Exception{
            "Cannot remove self node: " + std::string{name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    nodes_.erase(it);
}

auto NodeManager::get_node(std::string_view name) -> NodeEntry* {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) return nullptr;
    return &it->second;
}

auto NodeManager::get_node(std::string_view name) const -> const NodeEntry* {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) return nullptr;
    return &it->second;
}

auto NodeManager::has_node(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return nodes_.contains(std::string{name});
}

auto NodeManager::list_names() const -> std::vector<std::string> {
    std::lock_guard lock{mutex_};
    std::vector<std::string> names;
    names.reserve(nodes_.size());
    for (const auto& [name, _] : nodes_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

auto NodeManager::list_entries() const -> std::vector<NodeEntry> {
    std::lock_guard lock{mutex_};
    std::vector<NodeEntry> entries;
    entries.reserve(nodes_.size());
    for (const auto& [_, entry] : nodes_) {
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(),
              [](const NodeEntry& a, const NodeEntry& b) {
                  return a.node_name < b.node_name;
              });
    return entries;
}

auto NodeManager::self_node_id() const -> std::string {
    std::lock_guard lock{mutex_};
    return self_node_id_;
}

auto NodeManager::record_heartbeat(std::string_view name, const NodeMetrics& metrics,
                                   std::optional<NodeStatus> status) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    it->second.metrics = metrics;
    const auto now = std::chrono::system_clock::now();
    it->second.last_heartbeat_at = now;
    it->second.updated_at = now;
    if (status) {
        it->second.status = *status;
    } else if (it->second.status == NodeStatus::Offline) {
        it->second.status = NodeStatus::Online;
    }
}

auto NodeManager::sweep_stale_nodes(std::chrono::seconds ttl) -> void {
    std::lock_guard lock{mutex_};
    const auto now = std::chrono::system_clock::now();
    for (auto& [_, entry] : nodes_) {
        if (entry.is_self) continue;
        if (entry.status == NodeStatus::Draining || entry.status == NodeStatus::Maintenance) {
            continue;
        }
        if (entry.last_heartbeat_at.time_since_epoch().count() == 0) continue;
        if (now - entry.last_heartbeat_at > ttl) {
            entry.status = NodeStatus::Offline;
        }
    }
}

auto NodeManager::create_cluster(ClusterEntry entry, bool if_not_exists) -> void {
    if (entry.name.empty()) {
        throw common::Exception{
            "Cluster name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    std::lock_guard lock{mutex_};
    if (clusters_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Cluster already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    entry.created_at = std::chrono::system_clock::now();
    clusters_.emplace(entry.name, std::move(entry));
}

auto NodeManager::get_cluster(std::string_view name) const -> const ClusterEntry* {
    std::lock_guard lock{mutex_};
    auto it = clusters_.find(std::string{name});
    if (it == clusters_.end()) return nullptr;
    return &it->second;
}

auto NodeManager::list_clusters() const -> std::vector<ClusterEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ClusterEntry> out;
    out.reserve(clusters_.size());
    for (const auto& [_, c] : clusters_) {
        out.push_back(c);
    }
    return out;
}

auto NodeManager::increment_query_throughput(std::string_view name) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it != nodes_.end()) {
        ++it->second.metrics.query_throughput;
    }
}

auto NodeManager::assign_partition(std::string_view name, std::string_view partition_id)
    -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    it->second.partition_ids.emplace_back(std::string{partition_id});
    it->second.updated_at = std::chrono::system_clock::now();
}

auto NodeManager::assign_replica(std::string_view name, std::string_view replica_id) -> void {
    std::lock_guard lock{mutex_};
    auto it = nodes_.find(std::string{name});
    if (it == nodes_.end()) {
        throw common::Exception{
            "Unknown node: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    it->second.replica_ids.emplace_back(std::string{replica_id});
    it->second.updated_at = std::chrono::system_clock::now();
}

} // namespace mnemo::nodes
