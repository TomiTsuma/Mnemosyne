// src/Server/tcp_server.cpp — TCP server for MySQL-compatible protocol
// Mnemosyne: A column-oriented analytical DBMS
//
// NOTE: This is a placeholder. A full MySQL-compatible server would need
// protocol parsing, authentication, and result formatting.

#include "Server/tcp_server.h"
#include <iostream>

namespace mnesso::server {

TCPServer::TCPServer(std::shared_ptr<mnesso::interpreters::Context> ctx,
                     uint16_t port)
    : context_(std::move(ctx)), port_(port) {}

TCPServer::~TCPServer() {
    stop();
}

void TCPServer::start() {
    if (running_) return;
    running_ = true;
    std::cout << "[TCP] MySQL-compatible protocol server on port " << port_ << " (placeholder)\n";
}

void TCPServer::stop() {
    running_ = false;
}

bool TCPServer::is_running() const {
    return running_;
}

uint16_t TCPServer::port() const {
    return port_;
}

void TCPServer::shutdown() {
    stop();
}

} // namespace mnesso::server
