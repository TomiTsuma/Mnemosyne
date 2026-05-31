// src/Server/http_types.h — HTTP types (Request/Response) shared across server
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <sstream>
#include <memory>
#include <thread>
#include <atomic>
#include <winsock2.h>

namespace mnesso::server {

// Forward declare HTTPHandler for HTTPServer
class HTTPHandler;

// ── HTTP request ──
struct Request {
    std::string method;      // GET, POST, etc.
    std::string path;        // /query, /, /databases, etc.
    std::unordered_map<std::string, std::string> headers;
    std::string body;         // POST body
    std::unordered_map<std::string, std::string> query_params;
};

// ── HTTP response ──
struct Response {
    int status_code = 200;
    std::string body;
    std::string content_type = "text/plain";

    static auto ok(std::string body) -> Response {
        Response r;
        r.status_code = 200;
        r.body = std::move(body);
        r.content_type = "text/plain";
        return r;
    }

    static auto json(std::string body) -> Response {
        Response r;
        r.status_code = 200;
        r.body = std::move(body);
        r.content_type = "application/json";
        return r;
    }

    static auto error(int code, std::string body) -> Response {
        Response r;
        r.status_code = code;
        r.body = std::move(body);
        r.content_type = "text/plain";
        return r;
    }
};

// ── HTTPServer — lightweight HTTP server ──
class HTTPServer {
public:
    HTTPServer(std::shared_ptr<HTTPHandler> handler, uint16_t port, size_t backlog_size = 128);
    ~HTTPServer();

    HTTPServer(const HTTPServer&) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;

    void start();
    void stop();
    [[nodiscard]] auto is_running() const -> bool { return running_; }
    [[nodiscard]] auto port() const -> uint16_t { return port_; }

private:
    void accept_loop();

    std::shared_ptr<HTTPHandler> handler_;
    uint16_t port_;
    size_t backlog_size_;

#ifdef _WIN32
    SOCKET server_socket_ = INVALID_SOCKET;
#else
    int server_socket_ = -1;
#endif
    std::thread worker_thread_;
    std::atomic<bool> running_ = false;
};

} // namespace mnesso::server
