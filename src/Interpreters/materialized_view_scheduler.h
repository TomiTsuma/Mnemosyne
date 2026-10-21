// src/Interpreters/materialized_view_scheduler.h — background refresh of scheduled materialized views

#pragma once

#include "Interpreters/context.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

namespace mnemo::interpreters {

/// Refreshes every materialized view whose refresh policy is SCHEDULED (see
/// ALTER MATERIALIZED VIEW ... SET REFRESH EVERY) once its interval has passed since the last
/// refresh attempt. Views in every database are considered.
///
/// Each refresh runs under common::execution_mutex(), the lock the HTTP server holds around each
/// request, because the Context and the catalogs it reaches are not otherwise thread-safe. While
/// a view is refreshed the Context's current database is pointed at the view's database; it is
/// restored afterwards, so a client's USE is not disturbed.
///
/// A refresh that fails is logged and recorded on the view (status DEGRADED, last_error) and is
/// retried one interval later; it never stops the scheduler.
class MaterializedViewScheduler {
public:
    static auto instance() -> MaterializedViewScheduler&;

    void start(Context& context,
               std::chrono::milliseconds tick_interval = std::chrono::seconds{1});
    /// Stops the worker, waiting for a refresh already in progress to finish.
    void stop();

    /// Refreshes every view that is due now and returns how many it attempted.
    auto tick(Context& context) -> size_t;

private:
    MaterializedViewScheduler() = default;

    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex wake_mutex_;
    std::condition_variable wake_;
};

} // namespace mnemo::interpreters
