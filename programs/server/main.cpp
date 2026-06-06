// programs/server/main.cpp — Mnemosyne server entry point
// Mnemosyne: A column-oriented analytical DBMS

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

#include "Common/settings.h"
#include "Loggers/logger.h"
#include "Databases/database_factory.h"
#include "Storages/storage_factory.h"
#include "Interpreters/interpreter.h"
#include "Server/server.h"
#include "Coordination/coordination.h"
#include "Nodes/node_manager.h"
#include "Backups/backup.h"
#include "Databases/database_memory.h"
#include "Storages/file_storage.h"
#include "Loggers/target_console.h"

// ── Global state ──
std::shared_ptr<mnemo::interpreters::Context> g_context;
std::shared_ptr<mnemo::server::Server> g_server;
std::shared_ptr<mnemo::coordination::Coordination> g_coordination;
std::atomic<bool> g_running{true};

// ── Signal handling ──
void handle_signal(int sig) {
    mnemo::loggers::Logger::get_instance().fatal(
        std::format("Received signal {}", sig));
    g_running.store(false);
    if (g_server) g_server->stop();
}

// ── Main ──
auto main(int argc, char** argv) -> int {
    try {
        // Context manages its own settings internally.
        // Future: configure via global_context.get_settings().set(name, value)
        uint16_t http_port = 1143;
        if (argc > 1) {
            http_port = static_cast<uint16_t>(std::stoi(argv[1]));
        }

        // Initialize loggers
        auto& logger = mnemo::loggers::Logger::get_instance();
        logger.add_target(mnemo::loggers::ConsoleTarget::create());

        // Initialize databases
        mnemo::databases::DatabaseFactory::instance().register_engine(
            mnemo::databases::BuiltinEngines::MEMORY,
            []() {
                return mnemo::databases::DatabaseMemory::create("system", "");
            });

        // Initialize storages
        mnemo::storages::StorageFactory::instance().register_engine(
            mnemo::storages::BuiltinEngines::FILE,
            [](auto) {
                return mnemo::storages::FileStorage::create("default", "/tmp/mnemosyne");
            });
        // Setup signal handlers
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        // Create context and server
        g_context = std::make_shared<mnemo::interpreters::Context>(nullptr);
        auto server = std::make_shared<mnemo::server::Server>(g_context);
        g_server = server;
        g_running.store(true);
        server->start(http_port);

        const std::string self_addr = "local:" + std::to_string(http_port);
        g_coordination = mnemo::coordination::Coordination::create(
            "default", {self_addr}, self_addr);
        g_coordination->start();

        mnemo::loggers::Logger::get_instance().info("Mnemosyne server started");

        // Wait for shutdown
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        mnemo::loggers::Logger::get_instance().info("Mnemosyne server stopped");

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
