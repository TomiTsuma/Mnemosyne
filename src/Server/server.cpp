// src/Server/server.cpp — Server: manages HTTP and TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#include "Server/server.h"
#include "Server/http_server.h"
#include "Server/http_handler.h"
#include "Server/tcp_server.h"
#include <iostream>

namespace mnemo::server {

Server::Server(std::shared_ptr<interpreters::Context> ctx)
    : context_(std::move(ctx)) {}

Server::~Server() {
    stop();
}

void Server::start(uint16_t http_port, uint16_t tcp_port) {
    if (running_) return;
    running_ = true;

    // Create HTTP handler
    http_handler_ = std::make_shared<HTTPHandler>(*context_);

    // Create HTTP server
    http_server_ = std::make_shared<HTTPServer>(http_handler_, http_port);
    http_server_->start();
    std::cout << "[Server] HTTP API available at http://127.0.0.1:" << http_port << "\n";

    // Create TCP server
    tcp_server_ = std::make_shared<TCPServer>(context_, tcp_port);
    tcp_server_->start();
    std::cout << "[Server] MySQL-compatible protocol on port " << tcp_port << "\n";
}

void Server::stop() {
    if (!running_) return;
    running_ = false;

    if (http_server_) {
        http_server_->stop();
        http_server_.reset();
    }
    if (tcp_server_) {
        tcp_server_->stop();
        tcp_server_.reset();
    }
}

bool Server::is_running() const {
    return running_;
}

} // namespace mnemo::server
