// src/Interpreters/ddl_guard.cpp — DDLGuard implementation

#include "Interpreters/ddl_guard.h"

namespace mnemo::interpreters {

namespace {
std::mutex g_map_mutex;
std::unordered_map<std::string, std::weak_ptr<std::mutex>> g_locks;
} // namespace

auto DDLGuard::lock_key(std::string_view database, std::string_view table) -> std::string {
    return std::string{database} + "." + std::string{table};
}

auto DDLGuard::mutex_for(std::string_view key) -> std::shared_ptr<std::mutex> {
    std::lock_guard map_lock(g_map_mutex);
    auto& weak = g_locks[std::string{key}];
    auto mutex = weak.lock();
    if (!mutex) {
        mutex = std::make_shared<std::mutex>();
        weak  = mutex;
    }
    return mutex;
}

DDLGuard::DDLGuard(std::string database, std::string table)
    : lock_{*mutex_for(lock_key(database, table))} {}

DDLGuard::~DDLGuard() = default;

} // namespace mnemo::interpreters
