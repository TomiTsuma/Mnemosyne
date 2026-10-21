// src/Databases/view_catalog.h — View and materialized view catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Parsers/ast.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mnemo::databases {

struct ViewEntry {
    std::string name;
    parsers::QueryAST::Select definition;
    std::vector<std::string> dependencies;
    std::string status = "ACTIVE";
};

struct MaterializedViewEntry {
    std::string name;
    parsers::QueryAST::Select definition;
    std::string backing_table;
    std::vector<std::string> dependencies;
    // ACTIVE, or DEGRADED while the latest refresh attempt is failing.
    std::string status = "ACTIVE";

    // Refresh policy: 0 is MANUAL (only REFRESH MATERIALIZED VIEW refreshes it); otherwise the
    // MaterializedViewScheduler refreshes it once this many seconds have passed since the last
    // attempt. Set with ALTER MATERIALIZED VIEW ... SET REFRESH EVERY n <unit>.
    uint64_t refresh_interval_seconds = 0;
    // Both are set by every refresh, scheduled or manual, and by the initial population. The
    // schedule is measured from the last *attempt*, so a refresh that keeps failing is retried at
    // the same cadence as one that succeeds instead of on every scheduler tick.
    std::chrono::system_clock::time_point last_refreshed_at{};  // last success
    std::chrono::system_clock::time_point last_attempt_at{};
    std::string last_refresh_error;  // empty when the last attempt succeeded
};

/// Collect relation names referenced by a SELECT definition.
auto extract_dependencies(const parsers::QueryAST::Select& select)
    -> std::vector<std::string>;

} // namespace mnemo::databases
