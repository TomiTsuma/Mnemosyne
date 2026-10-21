// src/Interpreters/interpreter_alter_view.cpp — ALTER VIEW / ALTER MATERIALIZED VIEW

#include "Interpreters/interpreter_alter_view.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_select_query.h"
#include "Analyzer/analyzer.h"
#include "Databases/view_catalog.h"
#include "Common/exceptions.h"
#include <exception>
#include <memory>
#include <unordered_set>

namespace mnemo::interpreters {

namespace {

[[noreturn]] void fail(const std::string& message, common::ErrorCode code) {
    throw common::Exception{message, static_cast<int>(code)};
}

/// True if reading any of `dependencies` would, through a chain of views, read `target`.
auto reaches(const databases::Database& db, const std::vector<std::string>& dependencies,
             const std::string& target, std::unordered_set<std::string>& visited) -> bool {
    for (const auto& dependency : dependencies) {
        if (dependency == target) {
            return true;
        }
        if (!visited.insert(dependency).second) {
            continue;
        }
        if (const auto view = db.get_view(dependency)) {
            if (reaches(db, view->dependencies, target, visited)) {
                return true;
            }
        }
    }
    return false;
}

/// Checks a view definition against the current catalog without running it. Returns the
/// problems found, or an empty string if it is valid.
auto validate_definition(Context& context, const parsers::QueryAST::Select& definition)
    -> std::string {
    auto select = std::make_shared<parsers::QueryAST>();
    select->query_type = parsers::QueryAST::QueryType::SELECT;
    select->select = definition;

    analyzer::Analyzer analyzer{context};
    try {
        const auto result = analyzer.analyze(select);
        if (!analyzer.has_errors(result)) {
            return {};
        }
        std::string problems;
        for (const auto& error : result.errors) {
            problems += (problems.empty() ? "" : "; ") + error;
        }
        return problems;
    } catch (const std::exception& ex) {
        return ex.what();
    }
}

/// Runs a view definition and discards the result. Returns the error, or an empty string if it ran.
/// The analyzer cannot stand in for this: it lets a base table's columns show through a view, so
/// it accepts a SELECT of a column the view no longer outputs; only the executor rejects it.
auto try_execute(Context& context, const parsers::QueryAST::Select& definition) -> std::string {
    parsers::QueryAST select;
    select.query_type = parsers::QueryAST::QueryType::SELECT;
    select.select = definition;
    try {
        (void)InterpreterSelectQuery::execute(context, select);
        return {};
    } catch (const std::exception& ex) {
        return ex.what();
    }
}

/// Names what an object is when ALTER was aimed at the wrong kind, so the error is actionable.
auto describe_kind(const databases::Database& db, const std::string& name) -> std::string {
    if (db.has_view(name)) return "a view";
    if (db.has_materialized_view(name)) return "a materialized view";
    if (db.table_exists(name)) return "a table";
    return {};
}

} // namespace

auto InterpreterAlterView::execute_view(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    const auto& alter = query.alter;
    const auto& name = alter.table;

    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, name};

    auto db = ddl_utils::require_catalog(context);
    const auto existing = db->get_view(name);
    if (!existing) {
        const auto kind = describe_kind(*db, name);
        if (!kind.empty()) {
            fail("ALTER VIEW applies to views, but " + name + " is " + kind,
                 common::ErrorCode::LOGICAL_ERROR);
        }
        fail("Unknown view: " + name, common::ErrorCode::UNKNOWN_TABLE);
    }

    auto dependencies = databases::extract_dependencies(alter.view_definition);
    for (const auto& dependency : dependencies) {
        if (!db->table_exists(dependency) && !db->has_view(dependency) &&
            !db->has_materialized_view(dependency)) {
            fail("Unknown table or view in view definition: " + dependency,
                 common::ErrorCode::UNKNOWN_TABLE);
        }
    }

    // A view that reads itself, directly or through other views, could never be evaluated.
    std::unordered_set<std::string> visited;
    if (reaches(*db, dependencies, name, visited)) {
        fail("View definition would be circular: " + name + " would depend on itself",
             common::ErrorCode::LOGICAL_ERROR);
    }

    if (const auto problems = validate_definition(context, alter.view_definition);
        !problems.empty()) {
        fail("Invalid view definition: " + problems, common::ErrorCode::LOGICAL_ERROR);
    }

    databases::ViewEntry updated;
    updated.name = name;
    updated.definition = alter.view_definition;
    updated.dependencies = std::move(dependencies);
    updated.status = existing->status;
    db->replace_view(updated);

    // The new definition and every view that reads this one (directly or through other views)
    // must actually run against the new state; if not, put the old definition back and say what
    // would have broken. This executes them, so ALTER VIEW costs about as much as reading the
    // views once.
    try {
        if (const auto problem = try_execute(context, updated.definition); !problem.empty()) {
            fail("Invalid view definition: " + problem, common::ErrorCode::LOGICAL_ERROR);
        }

        std::string broken;
        for (const auto& other_name : db->view_names()) {
            if (other_name == name) {
                continue;
            }
            const auto other = db->get_view(other_name);
            std::unordered_set<std::string> seen;
            if (!other || !reaches(*db, other->dependencies, name, seen)) {
                continue;
            }
            if (const auto problem = try_execute(context, other->definition); !problem.empty()) {
                broken += (broken.empty() ? "" : "; ") + other_name + " (" + problem + ")";
            }
        }
        if (!broken.empty()) {
            fail("Cannot alter view " + name + ": it would break dependent views: " + broken,
                 common::ErrorCode::LOGICAL_ERROR);
        }
    } catch (...) {
        // Covers both the refusal above and anything unexpected thrown while checking.
        db->replace_view(*existing);
        throw;
    }

    return ddl_utils::make_ok_block();
}

auto InterpreterAlterView::execute_materialized_view(Context& context,
                                                     const parsers::QueryAST& query)
    -> core::Block {
    const auto& alter = query.alter;
    const auto& name = alter.table;

    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, name};

    auto db = ddl_utils::require_catalog(context);
    if (!db->has_materialized_view(name)) {
        const auto kind = describe_kind(*db, name);
        if (!kind.empty()) {
            fail("ALTER MATERIALIZED VIEW applies to materialized views, but " + name + " is " +
                     kind,
                 common::ErrorCode::LOGICAL_ERROR);
        }
        fail("Unknown materialized view: " + name, common::ErrorCode::UNKNOWN_TABLE);
    }

    db->set_materialized_view_refresh_interval(name, alter.refresh_every_seconds);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
