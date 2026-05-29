// src/Server/server.h — Server: coordinates HTTP and TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "context.h"
#include "http_server.h"
#include "tcp_server.h"
#include "loggers/logger.h"
#include <memory>
#include <string>

namespace mnesso::server {

// ── Server — high-level server manager ──
class Server {
public:
    Server(Context& context);

    // Start all servers
    auto start() -> bool;

    // Stop all servers
    auto stop() -> bool;

    // Check status
    [[nodiscard]] auto is_running() const -> bool;

    // Get configured ports
    [[nodiscard]] auto http_port() const -> uint16_t;
    [[nodiscard]] auto tcp_port()  const -> uint16_t;

    // Shutdown
    auto shutdown() -> void;

private:
    Context&         context_;
    std::unique_ptr<HTTPServer> http_server_;
    std::unique_ptr<TCPServer>  tcp_server_;
    bool                 running_ = false;
};

} // namespace mnesso::server
