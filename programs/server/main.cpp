// programs/server/main.cpp — Mnemosyne server entry point
// Mnemosyne: A column-oriented analytical DBMS

#include <iostream>
#include <string>
#include <csignal>
#include <chrono>
#include <thread>

#include "common/settings.h"
#include "loggers/logger.h"
#include "databases/database_factory.h"
#include "storages/storage_factory.h"
#include "interpretations/interpreter.h"
#include "server/server.h"
#include "coordination/coordination.h"
#include "backups/backup.h"

// ── Global context ──
mnesso::interpreters::Context global_context;

// ── Signal handling ──
void handle_signal(int sig) {
    mnesso::loggers::Logger::get_instance().fatal(
        std::format("Received signal {}", sig));
    mnesso::server::Server::instance().shutdown();
    std::exit(1);
}

// ── Main ──
auto main(int argc, char** argv) -> int {
    try {
        // Load settings
        auto settings = mnesso::common::Settings::defaults();
        if (argc > 1) {
            settings.load_from_file(argv[1]);
        }
        global_context.set_settings(settings);

        // Initialize loggers
        auto logger = mnesso::loggers::Logger::get_instance();
        logger.add_target(mnesso::loggers::ConsoleTarget::create());

        // Initialize databases
        mnesso::databases::DatabaseFactory::instance().register_engine(
            mnesso::databases::BuiltinEngines::MEMORY,
            []() {
                return mnesso::databases::DatabaseMemory::create("system", "");
            });

        // Initialize storages
        mnesso::storages::StorageFactory::instance().register_engine(
            mnesso::storages::BuiltinEngines::FILE,
            []() {
                return mnesso::storages::FileStorage::create("default", "/tmp/mnemosyne");
            });

        // Setup signal handlers
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        // Create and start servers
        auto server = std::make_unique<mnesso::server::Server>(global_context);
        server->start();

        mnesso::loggers::Logger::get_instance().info("Mnemosyne server started");

        // Wait for shutdown
        while (server->is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        mnesso::loggers::Logger::get_instance().info("Mnemosyne server stopped");

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
