// src/Server/http_server.h — HTTP server implementation
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "http_handler.h"
#include "context.h"
#include "loggers/logger.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

namespace mnesso::server {

// ── HTTPServer — listens on a port and routes requests ──
class HTTPServer {
public:
    HTTPServer(Context& context,
               std::string host = "127.0.0.1",
               uint16_t port = 8123);

    // Start / stop
    auto start() -> bool;
    auto stop() -> bool;
    [[nodiscard]] auto is_running() const -> bool;

    // Get port
    [[nodiscard]] auto port() const -> uint16_t;

    // Shutdown
    auto shutdown() -> void;

private:
    Context&             context_;
    std::string          host_;
    uint16_t             port_;
    bool                 running_ = false;
    std::atomic<bool>    running_atomic_ = false;
    std::unique_ptr<loggers::Logger> logger_;
    std::shared_ptr<HTTPHandler> handler_;
    std::mutex           mutex_;

    // Internal event loop
    void run_loop();
};

} // namespace mnesso::server
