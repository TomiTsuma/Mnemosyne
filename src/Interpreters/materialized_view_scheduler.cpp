// src/Interpreters/materialized_view_scheduler.cpp

#include "Interpreters/materialized_view_scheduler.h"
#include "Interpreters/interpreter_refresh_query.h"
#include "Databases/database.h"
#include "Common/execution_lock.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace mnemo::interpreters {

namespace {

struct DueView {
    std::string database;
    std::string name;
};

auto is_due(const databases::MaterializedViewEntry& entry,
            std::chrono::system_clock::time_point now) -> bool {
    return entry.refresh_interval_seconds != 0 &&
           now - entry.last_attempt_at >= std::chrono::seconds{entry.refresh_interval_seconds};
}

auto catalog_of(Context& context, const std::string& database)
    -> std::shared_ptr<databases::Database> {
    return std::dynamic_pointer_cast<databases::Database>(context.get_database(database));
}

/// Points the Context at another database for one refresh and puts it back, however it ends.
class CurrentDatabaseGuard {
public:
    CurrentDatabaseGuard(Context& context, std::string database)
        : context_(context), previous_(context.current_database()) {
        context_.set_current_database(std::move(database));
    }
    ~CurrentDatabaseGuard() { context_.set_current_database(previous_); }

    CurrentDatabaseGuard(const CurrentDatabaseGuard&) = delete;
    CurrentDatabaseGuard& operator=(const CurrentDatabaseGuard&) = delete;

private:
    Context& context_;
    std::string previous_;
};

} // namespace

auto MaterializedViewScheduler::instance() -> MaterializedViewScheduler& {
    static MaterializedViewScheduler scheduler;
    return scheduler;
}

auto MaterializedViewScheduler::tick(Context& context) -> size_t {
    std::vector<DueView> due;
    {
        std::lock_guard lock{common::execution_mutex()};
        const auto now = std::chrono::system_clock::now();
        for (const auto& database : context.databases()) {
            const auto catalog = catalog_of(context, database);
            if (!catalog) {
                continue;
            }
            for (const auto& name : catalog->materialized_view_names()) {
                const auto entry = catalog->get_materialized_view(name);
                if (entry && is_due(*entry, now)) {
                    due.push_back({database, name});
                }
            }
        }
    }

    size_t attempted = 0;
    for (const auto& view : due) {
        // One lock hold per view, so a long refresh delays queries by one view, not the whole
        // batch. Look again under the lock: the view may have been dropped, switched to MANUAL
        // or refreshed by hand since the list was built.
        std::lock_guard lock{common::execution_mutex()};
        const auto catalog = catalog_of(context, view.database);
        if (!catalog) {
            continue;
        }
        const auto entry = catalog->get_materialized_view(view.name);
        if (!entry || !is_due(*entry, std::chrono::system_clock::now())) {
            continue;
        }

        ++attempted;
        try {
            CurrentDatabaseGuard in_database{context, view.database};
            parsers::QueryAST refresh;
            refresh.query_type = parsers::QueryAST::QueryType::REFRESH;
            refresh.refresh.name = view.name;
            // Records success or failure on the view itself.
            (void)InterpreterRefreshQuery::execute(context, refresh);
        } catch (const std::exception& ex) {
            std::cerr << "[MaterializedViewScheduler] refresh of " << view.database << "."
                      << view.name << " failed: " << ex.what() << "\n";
        } catch (...) {
            std::cerr << "[MaterializedViewScheduler] refresh of " << view.database << "."
                      << view.name << " failed with an unknown error\n";
        }
    }
    return attempted;
}

void MaterializedViewScheduler::start(Context& context, std::chrono::milliseconds tick_interval) {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread([this, &context, tick_interval]() {
        std::unique_lock lock{wake_mutex_};
        while (running_) {
            lock.unlock();
            try {
                tick(context);
            } catch (const std::exception& ex) {
                std::cerr << "[MaterializedViewScheduler] tick failed: " << ex.what() << "\n";
            } catch (...) {
                std::cerr << "[MaterializedViewScheduler] tick failed with an unknown error\n";
            }
            lock.lock();
            wake_.wait_for(lock, tick_interval, [this]() { return !running_; });
        }
    });
}

void MaterializedViewScheduler::stop() {
    {
        std::lock_guard lock{wake_mutex_};
        running_ = false;
    }
    wake_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

} // namespace mnemo::interpreters
