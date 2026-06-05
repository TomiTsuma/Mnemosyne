// src/Server/tcp_server.h — TCP server for MySQL-compatible protocol
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Server/http_types.h"
#include <string>
#include <memory>
#include <atomic>
#include <cstdint>

namespace mnemo::interpreters { class Context; }

namespace mnemo::server {

// ── TCPServer — MySQL-compatible protocol server ──
class TCPServer {
public:
    TCPServer(std::shared_ptr<mnemo::interpreters::Context> ctx,
              uint16_t port);
    ~TCPServer();

    // Non-copyable
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    // Start listening
    void start();

    // Stop accepting connections
    void stop();

    // Check if server is running
    [[nodiscard]] auto is_running() const -> bool;

    // Get the port we're listening on
    [[nodiscard]] auto port() const -> uint16_t;

    // Shutdown everything
    void shutdown();

private:
    std::shared_ptr<mnemo::interpreters::Context> context_;
    uint16_t                               port_;
    std::atomic<bool>                      running_ = false;
};

} // namespace mnemo::server
