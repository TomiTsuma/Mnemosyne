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

auto NodeManager::sweep_stale_nodes(std::chrono::seconds ttl) -> std::vector<std::string> {
    std::lock_guard lock{mutex_};
    std::vector<std::string> went_offline;
    const auto now = std::chrono::system_clock::now();
    for (auto& [_, entry] : nodes_) {
        if (entry.is_self) continue;
        if (entry.status == NodeStatus::Draining || entry.status == NodeStatus::Maintenance) {
            continue;
        }
        if (entry.last_heartbeat_at.time_since_epoch().count() == 0) continue;
        if (now - entry.last_heartbeat_at > ttl) {
            if (entry.status != NodeStatus::Offline) {
                went_offline.push_back(entry.node_id);
            }
            entry.status = NodeStatus::Offline;
        }
    }
    return went_offline;
}

auto NodeManager::add_offline_callback(std::function<void(std::string_view)> callback) -> void {
    std::lock_guard lock{mutex_};
    offline_callbacks_.push_back(std::move(callback));
}

auto NodeManager::offline_callbacks() const -> std::vector<std::function<void(std::string_view)>> {
    std::lock_guard lock{mutex_};
    return offline_callbacks_;
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

auto NodeManager::drop_cluster(std::string_view name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = clusters_.find(std::string{name});
    if (it == clusters_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown cluster: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    // Detach member nodes so none is left pointing at a cluster that no longer exists.
    for (auto& [node_name, node] : nodes_) {
        if (node.cluster_id == it->first) {
            node.cluster_id.clear();
            node.updated_at = std::chrono::system_clock::now();
        }
    }
    clusters_.erase(it);
}

auto NodeManager::get_cluster(std::string_view name) const -> const ClusterEntry* {
    std::lock_guard lock{mutex_};
    auto it = clusters_.find(std::string{name});
    if (it == clusters_.end()) return nullptr;
    return &it->second;
}

auto NodeManager::alter_cluster_membership(std::string_view cluster_name,
                                           const std::vector<ClusterMembershipOp>& ops) -> void {
    std::lock_guard lock{mutex_};
    const std::string cluster{cluster_name};
    if (!clusters_.contains(cluster)) {
        throw common::Exception{
            "Unknown cluster: " + cluster,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    // Remember each touched node's original cluster_id so a failure part-way through can be undone.
    std::unordered_map<std::string, std::string> original;
    const auto restore = [&]() {
        for (const auto& [node_name, cluster_id] : original) {
            nodes_.at(node_name).cluster_id = cluster_id;
        }
    };

    try {
        for (const auto& op : ops) {
            auto it = nodes_.find(op.node);
            if (it == nodes_.end()) {
                throw common::Exception{
                    "Unknown node: " + op.node,
                    static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
            }
            original.emplace(op.node, it->second.cluster_id);

            if (op.add) {
                // A node belongs to one cluster. Refuse to move it silently: the caller must
                // REMOVE it from its current cluster first. A cluster_id that names no registered
                // cluster (the self node is seeded with "default", and ALTER NODE ... SET CLUSTER
                // does not validate) is not membership.
                const auto& current = it->second.cluster_id;
                if (!current.empty() && current != cluster && clusters_.contains(current)) {
                    throw common::Exception{
                        "Node " + op.node + " already belongs to cluster " + current
                            + "; remove it from that cluster first",
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
                it->second.cluster_id = cluster;
            } else {
                if (it->second.cluster_id != cluster) {
                    throw common::Exception{
                        "Node " + op.node + " is not a member of cluster " + cluster,
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
                it->second.cluster_id.clear();
            }
        }
    } catch (...) {
        restore();
        throw;
    }

    const auto now = std::chrono::system_clock::now();
    for (const auto& [node_name, cluster_id] : original) {
        auto& node = nodes_.at(node_name);
        if (node.cluster_id != cluster_id) {
            node.updated_at = now;
        }
    }
}

auto NodeManager::nodes_in_cluster(std::string_view cluster_name) const -> std::vector<NodeEntry> {
    std::lock_guard lock{mutex_};
    std::vector<NodeEntry> out;
    for (const auto& [node_name, node] : nodes_) {
        if (node.cluster_id == std::string{cluster_name}) {
            out.push_back(node);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const NodeEntry& a, const NodeEntry& b) { return a.node_name < b.node_name; });
    return out;
}

auto cluster_state_name(ClusterState state) -> std::string {
    switch (state) {
        case ClusterState::Creating: return "CREATING";
        case ClusterState::Online: return "ONLINE";
        case ClusterState::Degraded: return "DEGRADED";
        case ClusterState::Offline: return "OFFLINE";
    }
    return "UNKNOWN";
}

auto NodeManager::summarize_locked(const std::string& cluster_name) const -> ClusterSummary {
    ClusterSummary summary;
    summary.name = cluster_name;
    double cpu_sum = 0.0;
    for (const auto& [node_name, node] : nodes_) {
        if (node.cluster_id != cluster_name) continue;
        ++summary.node_count;
        if (node.status == NodeStatus::Online || node.status == NodeStatus::Busy) {
            ++summary.online_count;
        } else if (node.status == NodeStatus::Offline) {
            ++summary.offline_count;
        }
        summary.cpu_cores += node.resources.cpu_cores;
        summary.memory_bytes += node.resources.memory_bytes;
        summary.gpu_count += node.resources.gpu_count;
        summary.storage_bytes += node.resources.storage_bytes;
        cpu_sum += node.metrics.cpu_utilization_pct;
        summary.memory_used_bytes += node.metrics.memory_used_bytes;
        summary.storage_used_bytes += node.metrics.storage_used_bytes;
        summary.query_throughput += node.metrics.query_throughput;
        summary.pipeline_throughput += node.metrics.pipeline_throughput;
        summary.stream_throughput += node.metrics.stream_throughput;
    }
    if (summary.node_count > 0) {
        summary.avg_cpu_utilization_pct = cpu_sum / static_cast<double>(summary.node_count);
    }

    // No members: the cluster is still being assembled. All members offline: OFFLINE. All members
    // up: ONLINE. Anything else (a node that is offline, draining, registering, ...): DEGRADED.
    if (summary.node_count == 0) {
        summary.state = ClusterState::Creating;
    } else if (summary.offline_count == summary.node_count) {
        summary.state = ClusterState::Offline;
    } else if (summary.online_count == summary.node_count) {
        summary.state = ClusterState::Online;
    } else {
        summary.state = ClusterState::Degraded;
    }
    return summary;
}

auto NodeManager::cluster_summary(std::string_view cluster_name) const -> ClusterSummary {
    std::lock_guard lock{mutex_};
    const std::string key{cluster_name};
    if (!clusters_.contains(key)) {
        throw common::Exception{
            "Unknown cluster: " + key,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return summarize_locked(key);
}

auto NodeManager::cluster_summaries() const -> std::vector<ClusterSummary> {
    std::lock_guard lock{mutex_};
    std::vector<ClusterSummary> out;
    out.reserve(clusters_.size());
    for (const auto& [name, entry] : clusters_) {
        out.push_back(summarize_locked(name));
    }
    std::sort(out.begin(), out.end(),
              [](const ClusterSummary& a, const ClusterSummary& b) { return a.name < b.name; });
    return out;
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
