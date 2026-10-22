// src/Common/execution_lock.h — the one lock that serialises work on the shared Context

#pragma once

#include <mutex>

namespace mnemo::common {

/// interpreters::Context (current database, storages, databases) and the catalogs behind it
/// (Database views, StorageUnitManager, ...) are shared and mutable, and have no locking of
/// their own. Everything that executes against them must hold this mutex: the HTTP server takes
/// it around each request, and background workers (e.g. the materialized view refresh
/// scheduler) take it around each unit of work.
inline auto execution_mutex() -> std::mutex&
{
    static std::mutex mutex;
    return mutex;
}

} // namespace mnemo::common
