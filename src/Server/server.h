// src/Server/server.h — Server: manages HTTP and TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Server/http_server.h"
#include "Server/tcp_server.h"
#include "Interpreters/context.h"
#include <memory>
#include <string>

namespace mnesso::server {

// ── Server — top-level server manager ──
class Server {
public:
    explicit Server(std::shared_ptr<interpreters::Context> ctx);
    ~Server();

    // Start HTTP and TCP servers
    void start(uint16_t http_port = 8123, uint16_t tcp_port = 9000);

    // Stop both servers
    void stop();

    // Check if server is running
    [[nodiscard]] auto is_running() const -> bool;

private:
    std::shared_ptr<interpreters::Context> context_;
    std::shared_ptr<HTTPHandler>           http_handler_;
    std::shared_ptr<HTTPServer>            http_server_;
    std::shared_ptr<TCPServer>             tcp_server_;
    std::atomic<bool>                      running_ = false;
};

} // namespace mnesso::server
