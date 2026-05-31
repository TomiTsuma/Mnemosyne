// src/Interpreters/interpreter.h — SQL interpreter entry point
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Databases/database_manager.h"
#include "Planner/execution_plan.h"
#include "Processors/processor.h"
#include "Interpreters/context.h"
#include <memory>
#include <string_view>

namespace mnesso::interpreters {

// ── Plan — lightweight plan handle forwarded to the executor ──
struct Plan {
    std::shared_ptr<planner::PlanNode> root;

    // Construct from std::shared_ptr<ExecutionPlan> (extracts the root node)
    Plan(std::shared_ptr<planner::ExecutionPlan> exec_plan)
        : root(exec_plan ? exec_plan->root : nullptr) {}

    // Default / direct construction from a raw root pointer
    Plan() = default;
    explicit Plan(std::shared_ptr<planner::PlanNode> r) : root(std::move(r)) {}
};

// ── Interpreter — top-level SQL interpreter ──
class Interpreter {
public:
    explicit Interpreter(databases::DatabaseManager& db_manager);

    // Execute a SQL query string and return the result block
    auto execute(std::string_view query_text) -> core::Block;

    // Create a processor for a plan node
    auto create_processor(Plan plan, std::shared_ptr<Context> context)
        -> std::shared_ptr<processors::Processor>;

private:
    databases::DatabaseManager& db_manager_;
};

} // namespace mnesso::interpreters
