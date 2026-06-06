// src/Nodes/membership_service.cpp — Membership service implementation

#include "Nodes/membership_service.h"
#include "Nodes/node_manager.h"
#include <chrono>
#include <thread>

namespace mnemo::nodes {

auto MembershipService::instance() -> MembershipService& {
    static MembershipService inst;
    return inst;
}

void MembershipService::start(std::chrono::seconds sweep_interval,
                              std::chrono::seconds heartbeat_ttl) {
    if (running_.exchange(true)) return;
    sweep_interval_ = sweep_interval;
    heartbeat_ttl_ = heartbeat_ttl;
    sweep_thread_ = std::thread([this]() {
        while (running_) {
            const auto offline = NodeManager::instance().sweep_stale_nodes(heartbeat_ttl_);
            for (const auto& callback : NodeManager::instance().offline_callbacks()) {
                for (const auto& node_id : offline) {
                    callback(node_id);
                }
            }
            std::this_thread::sleep_for(sweep_interval_);
        }
    });
}

void MembershipService::stop() {
    running_ = false;
    if (sweep_thread_.joinable()) {
        sweep_thread_.join();
    }
}

auto MembershipService::is_running() const -> bool {
    return running_;
}

} // namespace mnemo::nodes
