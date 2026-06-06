// src/ReplicaGroups/replica_failover_service.h — Background replica failover watcher

#pragma once

#include <atomic>
#include <chrono>
#include <thread>

namespace mnemo::replica_groups {

class ReplicaFailoverService {
public:
    static auto instance() -> ReplicaFailoverService&;

    void start(std::chrono::seconds sweep_interval = std::chrono::seconds{5});
    void stop();
    [[nodiscard]] auto is_running() const -> bool;

private:
    ReplicaFailoverService() = default;

    std::atomic<bool> running_{false};
    std::chrono::seconds sweep_interval_{5};
    std::thread sweep_thread_;
};

} // namespace mnemo::replica_groups
