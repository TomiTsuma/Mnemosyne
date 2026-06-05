// programs/server/main.cpp — Mnemosyne server entry point
// Mnemosyne: A column-oriented analytical DBMS

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

#include "common/settings.h"
#include "loggers/logger.h"
#include "databases/database_factory.h"
#include "storages/storage_factory.h"
#include "interpreters/interpreter.h"
#include "server/server.h"
#include "coordination/coordination.h"
#include "backups/backup.h"
#include "databases/database_memory.h"
#include "storages/file_storage.h"
#include "loggers/target_console.h"

// ── Global state ──
std::shared_ptr<mnemo::interpreters::Context> g_context;
std::shared_ptr<mnemo::server::Server> g_server;
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
        (void)argc; (void)argv; // suppress unused warnings for now

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
        server->start();

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
