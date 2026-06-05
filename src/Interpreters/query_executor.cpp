// src/Interpreters/query_executor.cpp — Query executor implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Interpreters/query_executor.h"
#include "Interpreters/blockInterpreter.h"
#include "Interpreters/context.h"
#include "Planner/execution_plan.h"
#include "Common/exceptions.h"
#include <chrono>

namespace mnemo::interpreters {

// ── InterpreterFactory ──

auto InterpreterFactory::create(
    std::shared_ptr<planner::ExecutionPlan> plan,
    Context& context) -> std::shared_ptr<IInterpreter> {
    if (!plan || !plan->root) {
        throw common::Exception{
            "InterpreterFactory: invalid plan",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto node = plan->root;
    switch (node->type) {
        case planner::PlanNodeType::Source:
        case planner::PlanNodeType::Filter:
        case planner::PlanNodeType::Project:
        case planner::PlanNodeType::Aggregate:
        case planner::PlanNodeType::Sort:
        case planner::PlanNodeType::Limit:
        case planner::PlanNodeType::Union:
        case planner::PlanNodeType::Window:
            return create_select(std::move(plan), context);
        case planner::PlanNodeType::Join:
            return create_select(std::move(plan), context);
        case planner::PlanNodeType::HashTable:
            return create_select(std::move(plan), context);
        default:
            return create_ddl(std::move(plan), context);
    }
}

auto InterpreterFactory::create_select(
    std::shared_ptr<planner::ExecutionPlan> plan,
    Context& context) -> std::shared_ptr<IInterpreter> {
    return std::make_shared<BlockInterpreter>(std::move(plan), context);
}

auto InterpreterFactory::create_insert(
    std::shared_ptr<planner::ExecutionPlan> plan,
    Context& context) -> std::shared_ptr<IInterpreter> {
    return std::make_shared<BlockInterpreter>(std::move(plan), context);
}

auto InterpreterFactory::create_ddl(
    std::shared_ptr<planner::ExecutionPlan> plan,
    Context& context) -> std::shared_ptr<IInterpreter> {
    return std::make_shared<BlockInterpreter>(std::move(plan), context);
}

} // namespace mnemo::interpreters
