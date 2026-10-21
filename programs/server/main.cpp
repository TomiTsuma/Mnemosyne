// programs/server/main.cpp — Mnemosyne server entry point
// Mnemosyne: A column-oriented analytical DBMS

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <cstring>

#include "Common/settings.h"
#include "Loggers/logger.h"
#include "Databases/database_factory.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Databases/database.h"
#include "Storages/storage_factory.h"
#include "Storages/file_storage.h"
#include "Interpreters/interpreter.h"
#include "Interpreters/context.h"
#include "Server/server.h"
#include "Coordination/coordination.h"
#include "Nodes/node_manager.h"
#include "Backups/backup.h"
#include "Loggers/target_console.h"
#include "StorageUnits/storage_unit_catalog.h"
#include "StorageUnits/storage_unit_manager.h"
#include "DataTypes/data_type_factory.h"
#include "Disks/disk_local.h"

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

// ── Bootstrap helpers ──

static auto json_string_field(const std::string& json, const std::string& key) -> std::string
{
    const std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
}

static auto json_columns(const std::string& json)
    -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> result;
    auto arr_start = json.find("\"columns\":[");
    if (arr_start == std::string::npos) return result;
    arr_start += sizeof("\"columns\":[") - 1;
    auto arr_end = json.find(']', arr_start);
    if (arr_end == std::string::npos) return result;

    auto seg = json.substr(arr_start, arr_end - arr_start);
    size_t pos = 0;
    while (pos < seg.size()) {
        auto obj_s = seg.find('{', pos);
        if (obj_s == std::string::npos) break;
        auto obj_e = seg.find('}', obj_s);
        if (obj_e == std::string::npos) break;
        auto obj = seg.substr(obj_s, obj_e - obj_s + 1);
        auto name = json_string_field(obj, "name");
        auto type = json_string_field(obj, "type");
        if (!name.empty() && !type.empty()) {
            result.emplace_back(name, type);
        }
        pos = obj_e + 1;
    }
    return result;
}

// Scan ./data/ for _schema.json files and reconstruct file-backed tables in context.
static void bootstrap_file_tables(mnemo::interpreters::Context& context,
                                   std::shared_ptr<mnemo::disks::IDisk> default_disk)
{
    const std::string data_root = ".";  // disk path_ is "./data", so sub-paths are relative to it
    auto db_dirs = default_disk->list_dirs(data_root);
    size_t loaded = 0;

    for (const auto& db_dir : db_dirs) {
        const std::string db_name = db_dir;
        auto tbl_dirs = default_disk->list_dirs(db_dir);

        for (const auto& tbl_dir : tbl_dirs) {
            const std::string tbl_name    = tbl_dir;
            const std::string schema_path = db_dir + "/" + tbl_dir + "/_schema.json";

            if (!default_disk->exists(schema_path)) continue;

            const auto schema_size = default_disk->size(schema_path);
            if (schema_size == 0) continue;

            auto raw = default_disk->read(schema_path, 0, schema_size);
            if (!raw) continue;

            std::string json(reinterpret_cast<char*>(raw.get()), schema_size);
            auto col_defs = json_columns(json);
            if (col_defs.empty()) continue;

            // Ensure database is registered in context. Use the catalog Database (as
            // CREATE DATABASE does); DatabaseMemory rejects ENGINE=File tables.
            if (!context.get_database(db_name)) {
                auto& db_manager = mnemo::databases::DatabaseManager::instance();
                auto db = db_manager.get_database(db_name);
                if (!db) {
                    db = db_manager.create_database(db_name);
                }
                context.register_database(db_name, db);
            }
            auto idb = context.get_database(db_name);
            if (!idb) continue;

            // Reconstruct ordered column definitions
            std::vector<std::pair<std::string, mnemo::datatypes::DataTypePtr>> ordered_cols;
            std::unordered_map<std::string, mnemo::datatypes::DataTypePtr> col_map;
            for (const auto& [col_name, type_name] : col_defs) {
                auto dt = mnemo::datatypes::get_data_type(type_name);
                if (!dt) continue;
                ordered_cols.emplace_back(col_name, dt);
                col_map[col_name] = dt;
            }
            if (ordered_cols.empty()) continue;

            // Create storage and bind disk
            auto storage = idb->create_table(tbl_name, col_map, "File");
            auto file = std::dynamic_pointer_cast<mnemo::storages::FileStorage>(storage);
            if (!file) continue;

            const std::string rel_path = db_name + "/" + tbl_name;
            file->set_columns_ordered(ordered_cols);
            file->set_disk(default_disk);
            file->set_data_path(rel_path);
            file->set_storage_unit_name("su_default");
            file->sync_row_count();

            context.register_storage(tbl_name, storage);
            ++loaded;
            std::cout << "[Bootstrap] Loaded " << db_name << "." << tbl_name
                      << " (" << file->row_count() << " rows)\n";
        }
    }

    if (loaded > 0) {
        std::cout << "[Bootstrap] Restored " << loaded << " file-backed table(s)\n";
    }
}

// ── Main ──
auto main(int argc, char** argv) -> int {
    try {
        uint16_t http_port = 1143;
        if (argc > 1) {
            http_port = static_cast<uint16_t>(std::stoi(argv[1]));
        }

        // Initialize loggers
        auto& logger = mnemo::loggers::Logger::get_instance();
        logger.add_target(mnemo::loggers::ConsoleTarget::create());

        // Ensure the default data directory exists
        std::error_code ec;
        std::filesystem::create_directories("./data", ec);
        if (ec) {
            std::cerr << "[DBMS] Warning: could not create ./data: " << ec.message() << "\n";
        }

        // Register su_default storage unit → ./data/
        {
            auto& su_mgr = mnemo::storage_units::StorageUnitManager::instance();
            if (!su_mgr.has_unit("su_default")) {
                mnemo::storage_units::StorageUnitEntry entry;
                entry.name = "su_default";
                entry.type = mnemo::storage_units::StorageUnitType::Local;
                entry.path = "./data";
                su_mgr.create_unit(std::move(entry));
            }
        }

        // Build the disk object used for bootstrap (same root as su_default)
        auto default_disk = mnemo::disks::LocalFileDisk::create("su_default", "./data");

        // Initialize database engines
        mnemo::databases::DatabaseFactory::instance().register_engine(
            mnemo::databases::BuiltinEngines::MEMORY,
            []() {
                return mnemo::databases::DatabaseMemory::create("system", "");
            });

        // Initialize storage engines
        mnemo::storages::StorageFactory::instance().register_engine(
            mnemo::storages::BuiltinEngines::FILE,
            [](auto) {
                return mnemo::storages::FileStorage::create("default", "");
            });

        // Setup signal handlers
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        // Create global context
        g_context = std::make_shared<mnemo::interpreters::Context>(nullptr);

        // Bootstrap file-backed tables from any _schema.json on disk
        bootstrap_file_tables(*g_context, default_disk);

        // Start HTTP server
        auto server = std::make_shared<mnemo::server::Server>(g_context);
        g_server = server;
        g_running.store(true);
        server->start(http_port);

        const std::string self_addr = "local:" + std::to_string(http_port);
        g_coordination = mnemo::coordination::Coordination::create(
            "default", {self_addr}, self_addr);
        g_coordination->start();

        logger.info("Mnemosyne server started");

        // Wait for shutdown
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        logger.info("Mnemosyne server stopped");

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
