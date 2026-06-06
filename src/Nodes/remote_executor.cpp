// src/Nodes/remote_executor.cpp — Remote query execution via HTTP

#include "Nodes/remote_executor.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mnemo::nodes {

namespace {

auto http_get(const std::string& host, uint16_t port, const std::string& path) -> std::string {
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Connection: close\r\n\r\n";

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return {};
    }
#endif

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    const auto port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
#ifdef _WIN32
        WSACleanup();
#endif
        return {};
    }

#ifdef _WIN32
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        WSACleanup();
        return {};
    }
#else
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return {};
    }
#endif

    if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        freeaddrinfo(res);
        return {};
    }
    freeaddrinfo(res);

    const auto req = request.str();
#ifdef _WIN32
    send(sock, req.c_str(), static_cast<int>(req.size()), 0);
#else
    send(sock, req.data(), req.size(), 0);
#endif

    std::string response;
    char buf[4096];
    for (;;) {
#ifdef _WIN32
        const int n = recv(sock, buf, sizeof(buf), 0);
#else
        const ssize_t n = recv(sock, buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        response.append(buf, static_cast<size_t>(n));
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    const auto body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) return response;
    return response.substr(body_start + 4);
}

auto url_encode(const std::string& s) -> std::string {
    std::ostringstream encoded;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase << std::hex
                    << static_cast<int>(c >> 4) << static_cast<int>(c & 0x0F)
                    << std::nouppercase << std::dec;
        }
    }
    return encoded.str();
}

} // namespace

auto RemoteExecutor::build_node_url(std::string_view host, uint16_t port) -> std::string {
    return "http://" + std::string{host} + ":" + std::to_string(port);
}

auto RemoteExecutor::execute_on_node(std::string_view node_name, std::string_view sql)
    -> std::optional<core::Block> {
    const auto* entry = NodeManager::instance().get_node(node_name);
    if (!entry || entry->host.empty() || entry->port == 0) {
        throw common::Exception{
            "Node not reachable: " + std::string{node_name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    const auto path = "/query?query=" + url_encode(std::string{sql}) + "&format=JSON";
    const auto body = http_get(entry->host, entry->port, path);
    if (body.empty()) {
        return std::nullopt;
    }

    NodeManager::instance().increment_query_throughput(node_name);
    return core::Block{};
}

} // namespace mnemo::nodes
