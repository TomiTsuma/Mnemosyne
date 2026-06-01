// src/main.cpp — Mnemosyne DBMS entry point
// Mnemosyne: A column-oriented analytical DBMS

#include "Server/server.h"
#include "Server/http_server.h"
#include "Server/http_handler.h"
#include "Interpreters/context.h"
#include "Interpreters/query_executor.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Loggers/logger.h"
#include "Common/settings.h"
#include "Common/logging.h"
#include "Common/exceptions.h"
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
    auto& logger_ref = loggers::Logger::get_instance();
    auto logger = std::shared_ptr<loggers::Logger>{&logger_ref, [](loggers::Logger*){}};
    logger->set_level(loggers::LogLevel::INFO);

    // Initialize default database FIRST (Context needs it)
    auto& db_manager = databases::DatabaseManager::instance();
    auto db = db_manager.create_database("default");
    db_manager.register_idatabase("default", databases::DatabaseMemory::create("default", ""));
    std::cout << "[DBMS] Created default database\n";

    // Create global context (takes IDatabase from the manager)
    auto context = std::make_shared<interpreters::Context>(db_manager.get_idatabase("default"));
    context->set_current_database("default");

    // Create HTTPHandler (has a reference member, so use new + shared_ptr constructor)
    auto http_handler = std::shared_ptr<server::HTTPHandler>(new server::HTTPHandler(*context));
    server::HTTPServer http_server(http_handler, 8123);

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
