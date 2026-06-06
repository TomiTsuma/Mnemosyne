// src/Nodes/node_manager.h — Cluster-wide node registry

#pragma once

#include "Nodes/cluster_catalog.h"
#include "Nodes/node_catalog.h"
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::nodes {

class NodeManager {
public:
    static auto instance() -> NodeManager&;

    auto bootstrap_self(std::string_view node_id, std::string_view host, uint16_t port,
                        std::string_view cluster_id) -> void;

    auto create_node(NodeEntry entry, bool if_not_exists) -> void;
    auto register_node(std::string_view name, std::string_view host, uint16_t port) -> void;
    auto alter_node(std::string_view name, std::optional<NodeRole> role,
                    std::optional<NodeType> type, std::optional<NodeStatus> status,
                    std::optional<std::string_view> cluster_id) -> void;
    auto drain_node(std::string_view name) -> void;
    auto remove_node(std::string_view name, bool if_exists) -> void;

    auto get_node(std::string_view name) -> NodeEntry*;
    auto get_node(std::string_view name) const -> const NodeEntry*;
    [[nodiscard]] auto has_node(std::string_view name) const -> bool;
    [[nodiscard]] auto list_names() const -> std::vector<std::string>;
    [[nodiscard]] auto list_entries() const -> std::vector<NodeEntry>;
    [[nodiscard]] auto self_node_id() const -> std::string;

    auto record_heartbeat(std::string_view name, const NodeMetrics& metrics,
                          std::optional<NodeStatus> status) -> void;
    auto sweep_stale_nodes(std::chrono::seconds ttl) -> void;

    auto create_cluster(ClusterEntry entry, bool if_not_exists) -> void;
    auto get_cluster(std::string_view name) const -> const ClusterEntry*;
    [[nodiscard]] auto list_clusters() const -> std::vector<ClusterEntry>;

    auto increment_query_throughput(std::string_view name) -> void;
    auto assign_partition(std::string_view name, std::string_view partition_id) -> void;
    auto assign_replica(std::string_view name, std::string_view replica_id) -> void;

private:
    NodeManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, NodeEntry> nodes_;
    std::unordered_map<std::string, ClusterEntry> clusters_;
    std::string self_node_id_;
};

} // namespace mnemo::nodes
