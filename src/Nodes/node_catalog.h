// src/Nodes/node_catalog.h — NODE first-class entity catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::nodes {

enum class NodeType { Compute, Storage, Hybrid, Gpu };

enum class NodeRole { Coordinator, Worker, Observer };

enum class NodeStatus {
    Registering,
    Online,
    Busy,
    Degraded,
    Offline,
    Maintenance,
    Draining
};

[[nodiscard]] auto node_type_name(NodeType type) -> std::string;
[[nodiscard]] auto node_role_name(NodeRole role) -> std::string;
[[nodiscard]] auto node_status_name(NodeStatus status) -> std::string;
[[nodiscard]] auto parse_node_type(std::string_view name) -> NodeType;
[[nodiscard]] auto parse_node_role(std::string_view name) -> NodeRole;
[[nodiscard]] auto parse_node_status(std::string_view name) -> NodeStatus;
[[nodiscard]] auto capabilities_for_type(NodeType type) -> std::vector<std::string>;

struct NodeResources {
    uint32_t cpu_cores = 0;
    uint64_t memory_bytes = 0;
    uint32_t gpu_count = 0;
    uint64_t storage_bytes = 0;
    uint64_t network_bytes_per_sec = 0;
};

struct NodeMetrics {
    double cpu_utilization_pct = 0.0;
    uint64_t memory_used_bytes = 0;
    uint64_t storage_used_bytes = 0;
    uint64_t network_throughput_bytes = 0;
    uint64_t query_throughput = 0;
    uint64_t pipeline_throughput = 0;
    uint64_t stream_throughput = 0;
    uint32_t error_count = 0;
};

struct NodeEntry {
    std::string node_id;
    std::string node_name;
    std::string cluster_id;
    NodeType node_type = NodeType::Hybrid;
    NodeRole node_role = NodeRole::Worker;
    NodeStatus status = NodeStatus::Registering;
    std::string host;
    uint16_t port = 0;
    std::string version = "0.1.0";
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
    std::chrono::system_clock::time_point last_heartbeat_at{};
    NodeResources resources{};
    NodeMetrics metrics{};
    std::vector<std::string> capabilities;
    bool is_self = false;
    std::vector<std::string> partition_ids;
    std::vector<std::string> replica_ids;
};

void validate_node_entry(const NodeEntry& entry);

[[nodiscard]] auto detect_local_resources() -> NodeResources;

} // namespace mnemo::nodes
