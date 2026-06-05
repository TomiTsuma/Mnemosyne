// src/Interpreters/ddl_guard.h — Exclusive lock for DDL operations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mnemo::interpreters {

// ── DDLGuard — acquires a per-database/table mutex during schema changes ──
class DDLGuard {
public:
    DDLGuard(std::string database, std::string table);
    ~DDLGuard();

    DDLGuard(const DDLGuard&)            = delete;
    DDLGuard& operator=(const DDLGuard&) = delete;

private:
    static auto lock_key(std::string_view database, std::string_view table) -> std::string;
    static auto mutex_for(std::string_view key) -> std::shared_ptr<std::mutex>;

    std::unique_lock<std::mutex> lock_;
};

} // namespace mnemo::interpreters
