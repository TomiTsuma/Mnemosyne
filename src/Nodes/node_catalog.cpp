// src/Nodes/node_catalog.cpp — NODE catalog helpers

#include "Nodes/node_catalog.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>
#include <thread>

namespace mnemo::nodes {

namespace {

auto to_upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto node_type_name(NodeType type) -> std::string {
    switch (type) {
        case NodeType::Compute: return "COMPUTE";
        case NodeType::Storage: return "STORAGE";
        case NodeType::Hybrid:  return "HYBRID";
        case NodeType::Gpu:     return "GPU";
    }
    return "UNKNOWN";
}

auto node_role_name(NodeRole role) -> std::string {
    switch (role) {
        case NodeRole::Coordinator: return "COORDINATOR";
        case NodeRole::Worker:      return "WORKER";
        case NodeRole::Observer:    return "OBSERVER";
    }
    return "UNKNOWN";
}

auto node_status_name(NodeStatus status) -> std::string {
    switch (status) {
        case NodeStatus::Registering:  return "REGISTERING";
        case NodeStatus::Online:       return "ONLINE";
        case NodeStatus::Busy:         return "BUSY";
        case NodeStatus::Degraded:     return "DEGRADED";
        case NodeStatus::Offline:      return "OFFLINE";
        case NodeStatus::Maintenance:  return "MAINTENANCE";
        case NodeStatus::Draining:     return "DRAINING";
    }
    return "UNKNOWN";
}

auto parse_node_type(std::string_view name) -> NodeType {
    const auto upper = to_upper(std::string{name});
    if (upper == "COMPUTE") return NodeType::Compute;
    if (upper == "STORAGE") return NodeType::Storage;
    if (upper == "HYBRID") return NodeType::Hybrid;
    if (upper == "GPU") return NodeType::Gpu;
    throw common::Exception{
        "Unknown node type: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_node_role(std::string_view name) -> NodeRole {
    const auto upper = to_upper(std::string{name});
    if (upper == "COORDINATOR") return NodeRole::Coordinator;
    if (upper == "WORKER") return NodeRole::Worker;
    if (upper == "OBSERVER") return NodeRole::Observer;
    throw common::Exception{
        "Unknown node role: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto parse_node_status(std::string_view name) -> NodeStatus {
    const auto upper = to_upper(std::string{name});
    if (upper == "REGISTERING") return NodeStatus::Registering;
    if (upper == "ONLINE") return NodeStatus::Online;
    if (upper == "BUSY") return NodeStatus::Busy;
    if (upper == "DEGRADED") return NodeStatus::Degraded;
    if (upper == "OFFLINE") return NodeStatus::Offline;
    if (upper == "MAINTENANCE") return NodeStatus::Maintenance;
    if (upper == "DRAINING") return NodeStatus::Draining;
    throw common::Exception{
        "Unknown node status: " + std::string{name},
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto capabilities_for_type(NodeType type) -> std::vector<std::string> {
    switch (type) {
        case NodeType::Compute:
            return {"QUERY_ENGINE", "PIPELINES", "ML_INFERENCE"};
        case NodeType::Storage:
            return {"COLUMNAR_STORAGE", "ROW_STORAGE", "STREAMING"};
        case NodeType::Hybrid:
            return {"QUERY_ENGINE", "COLUMNAR_STORAGE", "STREAMING", "PIPELINES",
                    "SEARCH", "GRAPH", "VECTOR", "ML_INFERENCE"};
        case NodeType::Gpu:
            return {"ML_TRAINING", "ML_INFERENCE", "VECTOR"};
    }
    return {};
}

void validate_node_entry(const NodeEntry& entry) {
    if (entry.node_name.empty()) {
        throw common::Exception{
            "Node name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

auto detect_local_resources() -> NodeResources {
    NodeResources res;
    const auto hw = std::thread::hardware_concurrency();
    res.cpu_cores = hw > 0 ? static_cast<uint32_t>(hw) : 1;
    res.memory_bytes = 8ULL * 1024 * 1024 * 1024; // 8 GB default estimate
    res.gpu_count = 0;
    res.storage_bytes = 1024ULL * 1024 * 1024 * 1024; // 1 TB placeholder
    return res;
}

} // namespace mnemo::nodes
