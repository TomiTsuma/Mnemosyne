// src/main.cpp — Mnemosyne DBMS entry point
// Mnemosyne: A column-oriented analytical DBMS

#include "Server/server.h"
#include "Interpreters/context.h"
#include "Interpreters/query_executor.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Loggers/logger.h"
#include "Common/settings.h"
#include "Common/logging.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <thread>

namespace mnesso {

// ── Global shutdown handler ──
static volatile bool g_running = true;

void signal_handler(int signum) {
    std::cout << "\n[Server] Received signal " << signum << ", shutting down...\n";
    g_running = false;
}

} // namespace mnesso

int main(int argc, char* argv[]) {
    using namespace mnesso;

    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Setup logging
    auto logger = loggers::Logger::create();
    logger->add_target(loggers::TargetConsole::create());
    logger->set_level(loggers::LogLevel::Info);

    // Create global context
    auto context = std::make_shared<interpreters::Context>();
    context->set_current_database("default");

    // Initialize default database
    auto& db_manager = databases::DatabaseManager::instance();
    if (!db_manager.has_database("default")) {
        auto db = db_manager.create_database("default");
        db_manager.register_idatabase("default", databases::DatabaseMemory::create("default", ""));
        std::cout << "[DBMS] Created default database\n";
        (void)db;
    } else if (!db_manager.get_idatabase("default")) {
        db_manager.register_idatabase("default", databases::DatabaseMemory::create("default", ""));
    }

    // Create and start server
    server::HTTPHandler http_handler(context);
    server::HTTPServer http_server(std::make_shared<server::HTTPHandler>(http_handler), 8123);

    std::cout << "========================================\n";
    std::cout << "  Mnemosyne DBMS v0.1.0\n";
    std::cout << "  Column-oriented analytical database\n";
    std::cout << "========================================\n";
    std::cout << "[Server] Starting on port 8123 (HTTP)\n";
    std::cout << "[Server] Use http://localhost:8123 to connect\n";
    std::cout << "[Server] Available endpoints:\n";
    std::cout << "  GET  /ping           — health check\n";
    std::cout << "  GET  /query?query=   — execute SQL\n";
    std::cout << "  GET  /databases      — list databases\n";
    std::cout << "  GET  /tables/{db}    — list tables\n";
    std::cout << "  GET  /settings       — view settings\n";
    std::cout << "  GET  /metrics        — server metrics\n";
    std::cout << "  POST /query          — execute SQL\n";
    std::cout << "========================================\n";

    try {
        http_server.start();
    } catch (const common::Exception& e) {
        std::cerr << "[Server] Failed to start: " << e.what() << "\n";
        return 1;
    }

    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Cleanup
    http_server.stop();
    std::cout << "[Server] Shut down complete.\n";

    return 0;
}
