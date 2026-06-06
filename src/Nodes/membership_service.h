// src/Nodes/membership_service.h — Heartbeat monitoring and membership TTL sweep

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace mnemo::nodes {

class MembershipService {
public:
    static auto instance() -> MembershipService&;

    void start(std::chrono::seconds sweep_interval = std::chrono::seconds{5},
               std::chrono::seconds heartbeat_ttl = std::chrono::seconds{15});
    void stop();
    [[nodiscard]] auto is_running() const -> bool;

private:
    MembershipService() = default;

    std::atomic<bool> running_{false};
    std::thread sweep_thread_;
    std::chrono::seconds sweep_interval_{5};
    std::chrono::seconds heartbeat_ttl_{15};
};

} // namespace mnemo::nodes
