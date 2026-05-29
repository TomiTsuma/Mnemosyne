// src/Interpreters/query_executor.h — Interprets a query into execution results
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "execution_plan.h"
#include "context.h"
#include "common/block.h"
#include <memory>
#include <optional>

namespace mnesso::interpreters {

// ── QueryResult — output of a query ──
struct QueryResult {
    std::shared_ptr<core::Block> block;
    std::string      error;
    size_t           rows_read = 0;
    double           duration_ms = 0.0;
    size_t           bytes_read = 0;
    size_t           bytes_written = 0;
};

// ── IInterpreter — abstract query interpreter ──
class IInterpreter {
public:
    virtual ~IInterpreter() = default;

    // Execute and return result
    [[nodiscard]] virtual auto execute() -> QueryResult = 0;

    // Check if interpretation succeeded
    [[nodiscard]] virtual auto has_result() const -> bool = 0;
};

// ── InterpreterFactory — factory for creating interpreters ──
class InterpreterFactory {
public:
    static std::shared_ptr<IInterpreter> create(
        std::shared_ptr<planner::ExecutionPlan> plan,
        Context& context);

    // Specific interpreter constructors
    static std::shared_ptr<IInterpreter> create_select(
        std::shared_ptr<planner::ExecutionPlan> plan,
        Context& context);
    static std::shared_ptr<IInterpreter> create_insert(
        std::shared_ptr<planner::ExecutionPlan> plan,
        Context& context);
    static std::shared_ptr<IInterpreter> create_ddl(
        std::shared_ptr<planner::ExecutionPlan> plan,
        Context& context);
};

} // namespace mnesso::interpreters
