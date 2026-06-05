// src/Interpreters/blockInterpreter.h — Interpreter that operates on Block-level data
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "query_executor.h"
#include "context.h"
#include "execution_plan.h"
#include "Processors/processor.h"
#include <memory>

namespace mnemo::interpreters {

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

    // Helper methods
    auto create_processor_for_node(
        std::shared_ptr<planner::PlanNode> node,
        std::unordered_map<std::shared_ptr<planner::PlanNode>, std::shared_ptr<processors::Processor>>& proc_map)
        -> std::shared_ptr<processors::Processor>;

    static auto build_predicate(const std::string& expr)
        -> std::function<bool(const core::Field&)>;

    void execute_ddl_command(std::shared_ptr<planner::PlanNode> node);
};

} // namespace mnemo::interpreters
