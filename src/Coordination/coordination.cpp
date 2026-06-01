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

std::shared_ptr<Coordination> Coordination::create(
    std::string_view cluster_name,
    const std::vector<std::string>& node_addresses,
    std::string_view self_address) {
    auto coord = std::shared_ptr<Coordination>(new Coordination());
    coord->cluster_name_ = std::string{cluster_name};
    coord->self_address_ = std::string{self_address};

    // Parse "id:port" addresses into Node entries
    for (const auto& addr : node_addresses) {
        auto sep = addr.find(':');
        if (sep == std::string::npos) continue;
        std::string id = addr.substr(0, sep);
        std::string address = addr;
        coord->nodes_.push_back(Node{id, address});
    }

    return coord;
}

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
    // Compare the leader's node ID against our self node ID (first part of self_address_)
    auto self_sep = self_address_.find(':');
    std::string self_id = (self_sep != std::string::npos)
        ? self_address_.substr(0, self_sep) : self_address_;
    leader_ = (current_leader_ == self_id);
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