// src/Coordination/coordination.cpp — Coordination layer implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Coordination/coordination.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace mnemo::coordination {

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
    std::vector<Node> result;
    for (const auto& entry : nodes::NodeManager::instance().list_entries()) {
        std::string address = entry.host;
        if (!address.empty() && entry.port != 0) {
            address += ":" + std::to_string(entry.port);
        }
        result.push_back(Node{entry.node_id, address});
    }
    if (!result.empty()) {
        return result;
    }
    return nodes_;
}

void Coordination::add_node(const std::string& node_id, const std::string& address) {
    nodes_.push_back(Node{node_id, address});
    auto sep = address.find(':');
    const std::string host = (sep != std::string::npos) ? address.substr(0, sep) : address;
    const uint16_t port = (sep != std::string::npos)
        ? static_cast<uint16_t>(std::stoi(address.substr(sep + 1)))
        : uint16_t{0};
    if (!nodes::NodeManager::instance().has_node(node_id)) {
        nodes::NodeManager::instance().register_node(node_id, host, port);
    }
}

void Coordination::remove_node(const std::string& node_id) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
                       [&node_id](const Node& n) { return n.id == node_id; }),
        nodes_.end());
    nodes::NodeManager::instance().remove_node(node_id, true);
}

void Coordination::try_elect_leader() {
    const auto entries = nodes::NodeManager::instance().list_entries();
    if (entries.empty() && nodes_.empty()) {
        leader_ = false;
        return;
    }

    std::string leader_id;
    for (const auto& entry : entries) {
        if (entry.node_role == nodes::NodeRole::Coordinator &&
            (entry.status == nodes::NodeStatus::Online ||
             entry.status == nodes::NodeStatus::Busy)) {
            if (leader_id.empty() || entry.node_id < leader_id) {
                leader_id = entry.node_id;
            }
        }
    }
    if (leader_id.empty() && !entries.empty()) {
        leader_id = entries.front().node_id;
    }
    if (leader_id.empty() && !nodes_.empty()) {
        leader_id = nodes_.front().id;
    }

    current_leader_ = leader_id;
    const auto self_id = nodes::NodeManager::instance().self_node_id();
    if (!self_id.empty()) {
        leader_ = (current_leader_ == self_id);
        return;
    }
    auto self_sep = self_address_.find(':');
    std::string legacy_self = (self_sep != std::string::npos)
        ? self_address_.substr(0, self_sep) : self_address_;
    leader_ = (current_leader_ == legacy_self);
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

} // namespace mnemo::coordination