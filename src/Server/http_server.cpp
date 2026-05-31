// src/Server/http_server.cpp — HTTP server implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Server/http_server.h"
#include "Server/http_handler.h"
#include "Common/exceptions.h"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <sstream>
#include <cstring>

namespace mnesso::server {

// ── Simple HTTP request parser ──
struct ParsedRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> query_params;
};

static ParsedRequest parse_http_request(std::string_view raw) {
    ParsedRequest req;
    auto body_start = raw.find("\r\n\r\n");
    if (body_start == std::string_view::npos) {
        body_start = raw.find("\n\n");
    }

    std::string_view header_part;
    if (body_start != std::string_view::npos) {
        header_part = raw.substr(0, body_start);
        req.body = std::string(raw.substr(body_start + 4));
    } else {
        header_part = raw;
    }

    // Parse request line
    std::istringstream header_stream{std::string(header_part)};
    std::string line;
    if (std::getline(header_stream, line)) {
        // "GET /path?query HTTP/1.1"
        auto space1 = line.find(' ');
        if (space1 != std::string::npos) {
            req.method = line.substr(0, space1);
            auto rest = line.substr(space1 + 1);
            auto space2 = rest.find(' ');
            if (space2 != std::string::npos) {
                auto path_part = rest.substr(0, space2);
                auto qpos = path_part.find('?');
                if (qpos != std::string::npos) {
                    req.path = path_part.substr(0, qpos);
                    auto query = path_part.substr(qpos + 1);
                    // Parse query params
                    auto start = 0;
                    while (start < query.size()) {
                        auto amp = query.find('&', start);
                        if (amp == std::string::npos) amp = query.size();
                        auto eq = query.find('=', start);
                        if (eq != std::string::npos && eq < amp) {
                            req.query_params[query.substr(start, eq - start)] =
                                query.substr(eq + 1, amp - eq - 1);
                        } else {
                            req.query_params[query.substr(start, amp - start)] = "";
                        }
                        start = amp + 1;
                    }
                } else {
                    req.path = path_part;
                }
            }
        }
    }

    // Parse headers
    while (std::getline(header_stream, line)) {
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            auto key = line.substr(0, colon);
            auto val = line.substr(colon + 2); // skip ": "
            req.headers[key] = val;
        }
    }

    return req;
}

static std::string make_http_response(int status_code, std::string_view body,
                                       std::string_view content_type = "text/plain") {
    std::ostringstream oss;
    switch (status_code) {
        case 200: oss << "HTTP/1.1 200 OK\r\n"; break;
        case 400: oss << "HTTP/1.1 400 Bad Request\r\n"; break;
        case 404: oss << "HTTP/1.1 404 Not Found\r\n"; break;
        case 500: oss << "HTTP/1.1 500 Internal Server Error\r\n"; break;
        default:    oss << "HTTP/1.1 " << status_code << " Unknown\r\n"; break;
    }
    oss << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

// ── HTTPServer ──

HTTPServer::HTTPServer(std::shared_ptr<HTTPHandler> handler,
                       uint16_t port,
                       size_t backlog_size)
    : handler_{std::move(handler)},
      port_{port},
      backlog_size_{backlog_size} {
    // Initialize Winsock on Windows
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
}

HTTPServer::~HTTPServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HTTPServer::start() {
    if (running_) return;
    running_ = true;

    // Create listening socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET) {
        throw common::Exception{
            "HTTPServer: failed to create socket",
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind to port
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server_socket_);
        throw common::Exception{
            "HTTPServer: failed to bind to port " + std::to_string(port_),
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }

    // Listen
    if (listen(server_socket_, backlog_size_) == SOCKET_ERROR) {
        closesocket(server_socket_);
        throw common::Exception{
            "HTTPServer: failed to listen",
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }

    std::cout << "[HTTP] Listening on port " << port_ << "...\n";
    worker_thread_ = std::thread(&HTTPServer::accept_loop, this);
}

void HTTPServer::stop() {
    if (!running_) return;
    running_ = false;

    // Close the server socket to unblock accept
    if (server_socket_ != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(server_socket_);
#else
        close(server_socket_);
#endif
        server_socket_ = INVALID_SOCKET;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void HTTPServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        auto client_socket = accept(server_socket_,
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &client_len);
        if (client_socket == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }

        // Read request
        char buffer[8192];
        int bytes_read = 0;
        std::string raw_request;
        bool body_complete = false;

        while (!body_complete && running_) {
            bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) {
                body_complete = true;
                break;
            }
            buffer[bytes_read] = '\0';
            raw_request += std::string(buffer, bytes_read);

            // Check if we have the full request (look for end of headers)
            if (raw_request.find("\r\n\r\n") != std::string::npos ||
                raw_request.find("\n\n") != std::string::npos) {
                body_complete = true;
            }

            // Safety limit
            if (raw_request.size() > 65536) {
                body_complete = true;
            }
        }

        if (!raw_request.empty()) {
            // Parse request
            auto parsed = parse_http_request(raw_request);

            // Create request object
            server::Request http_req;
            http_req.method = parsed.method;
            http_req.path = parsed.path;
            http_req.headers = parsed.headers;
            http_req.body = parsed.body;
            http_req.query_params = parsed.query_params;

            // Handle request
            auto response = handler_->handle(http_req);

            // Send response
            auto resp_str = make_http_response(response.status_code, response.body);
            send(client_socket, resp_str.c_str(), resp_str.size(), 0);
        }

        // Close client socket
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
    }
}

} // namespace mnesso::server
