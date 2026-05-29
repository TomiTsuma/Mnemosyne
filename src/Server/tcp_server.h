// src/Server/tcp_server.h — TCP server for MySQL-compatible protocol
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "context.h"
#include "loggers/logger.h"
#include <string>
#include <memory>
#include <atomic>

namespace mnesso::server {

// ── TCPServer — MySQL-compatible protocol server ──
class TCPServer {
public:
    TCPServer(Context& context,
              std::string host = "127.0.0.1",
              uint16_t port = 9000);

    // Start / stop
    auto start() -> bool;
    auto stop() -> bool;
    [[nodiscard]] auto is_running() const -> bool;

    [[nodiscard]] auto port() const -> uint16_t;
    auto shutdown() -> void;

private:
    Context&             context_;
    std::string          host_;
    uint16_t             port_;
    bool                 running_ = false;
    std::atomic<bool>    running_atomic_ = false;
    std::unique_ptr<loggers::Logger> logger_;
    std::mutex           mutex_;

    void run_loop();
};

} // namespace mnesso::server
