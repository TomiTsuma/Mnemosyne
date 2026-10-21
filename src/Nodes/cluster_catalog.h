// src/Nodes/cluster_catalog.h — CLUSTER metadata (minimal, PRD alignment)

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mnemo::nodes {

struct ClusterEntry {
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point created_at{};
};

// One ADD NODE / REMOVE NODE step of an ALTER CLUSTER statement.
struct ClusterMembershipOp {
    bool add = true;  // false = REMOVE
    std::string node;
};

// Subset of the PRD cluster states (CLUSTER_.md §8) that can be derived from member health.
// UPGRADING and MAINTENANCE are operator-set states and are not modelled yet.
enum class ClusterState { Creating, Online, Degraded, Offline };

[[nodiscard]] auto cluster_state_name(ClusterState state) -> std::string;

// A point-in-time roll-up of a cluster's member nodes, computed in one locked pass so the
// counts and aggregates are consistent with each other.
struct ClusterSummary {
    std::string name;
    ClusterState state = ClusterState::Creating;
    size_t node_count = 0;
    size_t online_count = 0;   // members that are ONLINE or BUSY
    size_t offline_count = 0;  // members that are OFFLINE

    // Capacity, summed over members.
    uint32_t cpu_cores = 0;
    uint64_t memory_bytes = 0;
    uint32_t gpu_count = 0;
    uint64_t storage_bytes = 0;

    // Utilisation, averaged (CPU) or summed (the rest) over members.
    double avg_cpu_utilization_pct = 0.0;
    uint64_t memory_used_bytes = 0;
    uint64_t storage_used_bytes = 0;
    uint64_t query_throughput = 0;
    uint64_t pipeline_throughput = 0;
    uint64_t stream_throughput = 0;
};

} // namespace mnemo::nodes
