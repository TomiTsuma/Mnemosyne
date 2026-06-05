// src/Interpreters/context.h — Query context management
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Common/settings.h"
#include "Databases/i_database.h"
#include "Storages/i_storage.h"
#include "Common/thread_pool.h"
#include "Loggers/logger.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace mnemo::interpreters {

// ── Query context — carries all state for a single query ──
class Context {
public:
    struct TableInfo {
        std::string name;
        std::string database;
    };

    struct ColumnInfo {
        std::string table;
        std::string data_type;
    };

    struct QueryInfo {
        std::string   user;
        std::string   query_id;
        std::string   initial_query_id;
        std::string   current_database;
        std::string   current_role;
        size_t        memory_usage = 0;
        size_t        max_memory_usage = 0;
        size_t        max_rows = 0;
        double        query_duration_ms = 0;
    };

    // Get the global settings
    auto& get_settings() { return settings_; }
    [[nodiscard]] auto get_settings() const -> const common::Settings& { return settings_; }

    // Get the pool
    auto& pool() { return *pool_; }
    auto& logger() { return *logger_; }

    // Get/set database
    auto& current_database() { return current_db_; }
    void  set_current_database(std::string name) { current_db_ = std::move(name); }

    // Get/set query info
    auto& query_info() { return query_info_; }
    void  set_query_info(QueryInfo info) { query_info_ = std::move(info); }

    // Get storage by name (within current database)
    [[nodiscard]] auto get_storage(std::string_view name)
        -> std::shared_ptr<storages::IStorage>;

    // Register a storage
    void register_storage(std::string name, std::shared_ptr<storages::IStorage> storage);

    // Remove a storage from the session catalog
    void unregister_storage(std::string_view name);

    // Register a database
    void register_database(std::string name, std::shared_ptr<databases::IDatabase> db);

    // Get registered database
    [[nodiscard]] auto get_database(std::string_view name)
        -> std::shared_ptr<databases::IDatabase>;

    // List all databases
    [[nodiscard]] auto databases() const -> std::vector<std::string>;

    // Memory tracking
    void track_memory(size_t bytes);
    void untrack_memory(size_t bytes);
    [[nodiscard]] auto total_memory() const -> size_t;

    // Check if a role exists
    [[nodiscard]] auto has_role(std::string_view name) const -> bool;

    // Get a setting by name
    [[nodiscard]] auto get_setting(std::string_view name)
        -> std::optional<common::SettingValueType>;

    // Set a setting
    void set_setting(std::string_view name, common::SettingValueType value);

    // Construct from a database (registers it)
    explicit Context(std::shared_ptr<databases::IDatabase> db);

    // Default constructor — deleted in production, enabled in tests via
    // #define BEFORE including context.h  (e.g. in test setup)
#ifdef ALLOW_CONTEXT_DEFAULT_CTOR
    Context() = default;
#else
    Context() = delete;
#endif

private:
    common::Settings           settings_;
    // Always-valid worker pool. A default member initializer guarantees pool_ is
    // non-null for every constructor (including the defaulted test constructor),
    // so pool() never dereferences a null unique_ptr.
    std::unique_ptr<common::ThreadPool> pool_ = std::make_unique<common::ThreadPool>();
    std::unique_ptr<loggers::Logger> logger_;
    std::string                current_db_;
    QueryInfo                  query_info_;
    std::unordered_map<std::string, std::shared_ptr<databases::IDatabase>> databases_;
    std::unordered_map<std::string, std::shared_ptr<storages::IStorage>>   storages_;
    size_t                     memory_tracked_ = 0;
    std::unordered_map<std::string, bool>                                    roles_;
};

// ── Global context — singleton accessible from anywhere ──
Context& get_global_context();

} // namespace mnemo::interpreters
