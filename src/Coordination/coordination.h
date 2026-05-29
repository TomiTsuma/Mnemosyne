// src/Coordination/coordination.h — Coordination: distributed consensus layer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

namespace mnesso::coordination {

// ── Node info ──
struct NodeInfo {
    std::string host;
    uint16_t    port;
    uint64_t    node_id;
    std::string role; // LEADER, FOLLOWER, CANDIDATE
};

// ── Event types ──
enum class EventType : uint8_t {
    LEADER_ELECTED,
    NODE_JOINED,
    NODE_LEFT,
    CONFIG_CHANGE,
    LOG_SYNC,
    SHUTDOWN,
};

// ── Event ──
struct Event {
    EventType type;
    NodeInfo  from;
    std::string message;
    uint64_t  term;
};

// ── Coordination — Raft/Zab-based consensus layer ──
class Coordination {
public:
    static auto create(std::string cluster_id,
                       std::vector<std::string> nodes,
                       std::string self_node)
        -> std::shared_ptr<Coordination>;

    // Lifecycle
    auto start() -> bool;
    auto stop() -> bool;
    [[nodiscard]] auto is_leader() const -> bool;
    [[nodiscard]] auto get_leader() const -> std::optional<NodeInfo>;

    // State machine
    auto apply(std::string command) -> bool;
    [[nodiscard]] auto query(std::string key) -> std::optional<std::string>;

    // Configuration
    auto add_node(std::string node) -> bool;
    auto remove_node(std::string node) -> bool;

    // Event handling
    void set_event_handler(std::function<void(Event)> handler);
    auto events() const -> std::vector<Event>;

private:
    Coordination();
    std::string                 cluster_id_;
    std::vector<std::string>    nodes_;
    std::string                 self_node_;
    bool                        leader_ = false;
    std::optional<NodeInfo>     leader_info_;
    std::unordered_map<std::string, std::string> state_machine_;
    std::vector<Event>          events_;
    std::function<void(Event)>  event_handler_;
    mutable std::mutex          mutex_;
};

} // namespace mnesso::coordination
