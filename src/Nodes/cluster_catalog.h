// src/Nodes/cluster_catalog.h — CLUSTER metadata (minimal, PRD alignment)

#pragma once

#include <chrono>
#include <string>

namespace mnemo::nodes {

struct ClusterEntry {
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point created_at{};
};

} // namespace mnemo::nodes
