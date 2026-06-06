// src/ReplicaGroups/replica_failover_service.cpp

#include "ReplicaGroups/replica_failover_service.h"
#include "ReplicaGroups/replica_group_manager.h"
#include <thread>

namespace mnemo::replica_groups {

auto ReplicaFailoverService::instance() -> ReplicaFailoverService& {
    static ReplicaFailoverService inst;
    return inst;
}

void ReplicaFailoverService::start(std::chrono::seconds sweep_interval) {
    if (running_.exchange(true)) {
        return;
    }
    sweep_interval_ = sweep_interval;
    sweep_thread_ = std::thread([this]() {
        while (running_) {
            ReplicaGroupManager::instance().sweep_node_status();
            std::this_thread::sleep_for(sweep_interval_);
        }
    });
}

void ReplicaFailoverService::stop() {
    running_ = false;
    if (sweep_thread_.joinable()) {
        sweep_thread_.join();
    }
}

auto ReplicaFailoverService::is_running() const -> bool {
    return running_;
}

} // namespace mnemo::replica_groups
