// src/Interpreters/blockInterpreter.h — Interpreter that operates on Block-level data
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "query_executor.h"
#include "context.h"
#include "execution_plan.h"
#include "processors/processor.h"
#include <memory>

namespace mnesso::interpreters {

// ── BlockInterpreter — interprets a plan into a pipeline of processors ──
class BlockInterpreter final : public IInterpreter {
public:
    BlockInterpreter(std::shared_ptr<planner::ExecutionPlan> plan,
                     Context& context);

    [[nodiscard]] auto execute() -> QueryResult override;
    [[nodiscard]] auto has_result() const -> bool override;

    // Build processor pipeline from plan
    auto build_pipeline() -> std::shared_ptr<processors::Processor>;

private:
    std::shared_ptr<planner::ExecutionPlan> plan_;
    Context&                                context_;
    std::shared_ptr<processors::Processor>  pipeline_;
    QueryResult                             result_;
};

} // namespace mnesso::interpreters
