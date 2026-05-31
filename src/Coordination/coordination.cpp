// src/Coordination/coordination.cpp — Coordination layer implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Coordination/coordination.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace mnesso::coordination {

// ── Coordination ──

Coordination::Coordination() : running_{false}, leader_{false} {}

void Coordination::start() {
    if (running_) return;
    running_ = true;
    // Start leader election thread
    election_thread_ = std::thread([this]() {
        while (running_) {
            try_elect_leader();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void Coordination::stop() {
    running_ = false;
    if (election_thread_.joinable()) {
        election_thread_.join();
    }
}

bool Coordination::is_running() const {
    return running_;
}

bool Coordination::is_leader() const {
    return leader_;
}

auto Coordination::get_nodes() const -> std::vector<Node> {
    return nodes_;
}

void Coordination::add_node(const std::string& node_id, const std::string& address) {
    nodes_.push_back(Node{node_id, address});
}

void Coordination::remove_node(const std::string& node_id) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
                       [&node_id](const Node& n) { return n.id == node_id; }),
        nodes_.end());
}

void Coordination::try_elect_leader() {
    if (nodes_.empty()) {
        leader_ = false;
        return;
    }

    // Simple leader election: pick the first node
    current_leader_ = nodes_[0].id;
    leader_ = (current_leader_ == "self");
}

auto Coordination::get_lock(std::string_view lock_name) -> std::shared_ptr<Lock> {
    std::string key{lock_name};
    auto it = locks_.find(key);
    if (it == locks_.end()) {
        auto lock = std::make_shared<Lock>();
        locks_[key] = lock;
        return lock;
    }
    return it->second;
}

} // namespace mnesso::coordination