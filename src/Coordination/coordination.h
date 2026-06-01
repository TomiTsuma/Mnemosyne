// src/Coordination/coordination.h — Coordination: distributed consensus layer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <thread>

namespace mnesso::coordination {

// ── Node descriptor ──
struct Node {
    std::string id;
    std::string address;
};

// ── Coordination lock handle ──
class Lock {
public:
    Lock() = default;
    ~Lock() = default;
};

// ── Coordination — lightweight local coordination manager ──
class Coordination {
public:
    Coordination();

    // Factory
    static std::shared_ptr<Coordination> create(
        std::string_view cluster_name,
        const std::vector<std::string>& node_addresses,
        std::string_view self_address);

    // Lifecycle
    void start();
    void stop();
    [[nodiscard]] auto is_running() const -> bool;
    [[nodiscard]] auto is_leader() const -> bool;

    // Nodes
    auto get_nodes() const -> std::vector<Node>;
    void add_node(const std::string& node_id, const std::string& address);
    void remove_node(const std::string& node_id);

    // Locking
    auto get_lock(std::string_view lock_name) -> std::shared_ptr<Lock>;

private:
    void try_elect_leader();

    std::string cluster_name_;
    std::string self_address_;
    bool running_ = false;
    bool leader_ = false;
    std::thread election_thread_;
    std::vector<Node> nodes_;
    std::string current_leader_;
    std::unordered_map<std::string, std::shared_ptr<Lock>> locks_;
    mutable std::mutex mutex_;
};

} // namespace mnesso::coordination
