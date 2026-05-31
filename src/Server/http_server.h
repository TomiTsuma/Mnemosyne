// src/Server/http_server.h — HTTP server implementation
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Server/http_types.h"
#include <string>
#include <string_view>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace mnesso::server {

// ── HTTPServer — simple threaded HTTP server ──
class HTTPServer {
public:
    // Create server — handler will be called for each request
    HTTPServer(std::shared_ptr<HTTPHandler> handler,
               uint16_t port,
               size_t backlog_size = 128);
    ~HTTPServer();

    // Non-copyable
    HTTPServer(const HTTPServer&) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;

    // Start listening
    void start();

    // Stop accepting connections
    void stop();

    // Check if server is running
    [[nodiscard]] auto is_running() const -> bool { return running_; }

    // Get the port we're listening on
    [[nodiscard]] auto port() const -> uint16_t { return port_; }

private:
    // Accept loop — runs in its own thread
    void accept_loop();

    std::shared_ptr<HTTPHandler> handler_;
    uint16_t                     port_;
    size_t                       backlog_size_;

    // Socket state
#ifdef _WIN32
    SOCKET server_socket_ = INVALID_SOCKET;
#else
    int server_socket_ = -1;
#endif
    std::thread    worker_thread_;
    std::atomic<bool> running_ = false;
};

} // namespace mnesso::server
