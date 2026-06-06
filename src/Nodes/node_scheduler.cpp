// src/Nodes/node_scheduler.cpp — Node scheduler implementation

#include "Nodes/node_scheduler.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include <algorithm>

namespace mnemo::nodes {

namespace {

auto has_capabilities(const NodeEntry& entry,
                      const std::vector<std::string>& required) -> bool {
    for (const auto& cap : required) {
        const auto it = std::find(entry.capabilities.begin(), entry.capabilities.end(), cap);
        if (it == entry.capabilities.end()) {
            return false;
        }
    }
    return true;
}

auto is_schedulable(NodeStatus status) -> bool {
    return status == NodeStatus::Online || status == NodeStatus::Busy;
}

} // namespace

auto NodeScheduler::instance() -> NodeScheduler& {
    static NodeScheduler inst;
    return inst;
}

auto NodeScheduler::pick_node(std::string_view workload_type,
                              const std::vector<std::string>& required_capabilities)
    -> std::optional<std::string> {
    (void)workload_type;
    auto& mgr = NodeManager::instance();
    const auto self_id = mgr.self_node_id();
    const auto entries = mgr.list_entries();

    // Prefer self for locality when capable
    if (!self_id.empty()) {
        if (const auto* self = mgr.get_node(self_id)) {
            if (is_schedulable(self->status) && has_capabilities(*self, required_capabilities)) {
                return self_id;
            }
        }
    }

    for (const auto& entry : entries) {
        if (!is_schedulable(entry.status)) continue;
        if (!has_capabilities(entry, required_capabilities)) continue;
        return entry.node_name;
    }
    return std::nullopt;
}

auto NodeScheduler::pick_gpu_node() -> std::optional<std::string> {
    return pick_node("ml", {"ML_TRAINING"});
}

auto NodeScheduler::pick_remote_node(const std::vector<std::string>& required_capabilities)
    -> std::optional<std::string> {
    const auto self_id = nodes::NodeManager::instance().self_node_id();
    for (const auto& entry : nodes::NodeManager::instance().list_entries()) {
        if (entry.node_name == self_id) continue;
        if (!is_schedulable(entry.status)) continue;
        if (!has_capabilities(entry, required_capabilities)) continue;
        return entry.node_name;
    }
    return std::nullopt;
}

} // namespace mnemo::nodes
