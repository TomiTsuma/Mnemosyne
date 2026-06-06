// src/Nodes/node_scheduler.h — Workload placement by capability and health

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mnemo::nodes {

class NodeScheduler {
public:
    static auto instance() -> NodeScheduler&;

    [[nodiscard]] auto pick_node(std::string_view workload_type,
                                 const std::vector<std::string>& required_capabilities)
        -> std::optional<std::string>;

    [[nodiscard]] auto pick_gpu_node() -> std::optional<std::string>;
    [[nodiscard]] auto pick_remote_node(const std::vector<std::string>& required_capabilities)
        -> std::optional<std::string>;

private:
    NodeScheduler() = default;
};

} // namespace mnemo::nodes
