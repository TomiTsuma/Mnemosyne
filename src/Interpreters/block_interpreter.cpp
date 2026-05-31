// src/Interpreters/block_interpreter.cpp — Block-level interpreter implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Interpreters/blockInterpreter.h"
#include "Interpreters/context.h"
#include "Planner/execution_plan.h"
#include "Processors/processor.h"
#include "Processors/processors_source.h"
#include "Processors/i_input_stream.h"
#include "Processors/i_output_stream.h"
#include "Core/block.h"
#include "Common/exceptions.h"
#include <chrono>

namespace mnesso::interpreters {

BlockInterpreter::BlockInterpreter(std::shared_ptr<planner::ExecutionPlan> plan,
                                   Context& context)
    : plan_(std::move(plan)), context_(context) {}

auto BlockInterpreter::build_pipeline() -> std::shared_ptr<processors::Processor> {
    if (!plan_ || !plan_->root) {
        throw common::Exception{
            "BlockInterpreter: null plan",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    // Walk the plan tree bottom-up and create processors
    // For now, just create a simple processor from the root node
    auto root = plan_->root;

    if (root->children.empty()) {
        // Leaf node — create source processor
        return std::make_shared<processors::EmptyBlockSource>();
    }

    // Build children first (recursive)
    std::vector<std::shared_ptr<processors::Processor>> children;
    for (auto& child : root->children) {
        auto child_pipe = std::make_shared<processors::EmptyBlockSource>();
        children.push_back(child_pipe);
    }

    // Connect children to parent based on node type
    // For now, return the root processor
    return std::make_shared<processors::EmptyBlockSource>();
}

auto BlockInterpreter::execute() -> QueryResult {
    QueryResult result;

    try {
        auto start = std::chrono::steady_clock::now();

        // Build processor pipeline
        pipeline_ = build_pipeline();
        if (!pipeline_) {
            result.error = "Failed to build processor pipeline";
            return result;
        }

        // Start the pipeline
        pipeline_->start();

        // Get header — getHeader() returns Block by value (never null)
        core::Block header = pipeline_->getHeader();

        // Execute and collect blocks
        // For now, just return the header as the result
        result.block = std::make_shared<core::Block>(std::move(header));
        result.rows_read = result.block->row_count();

        auto end = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    } catch (const common::Exception& e) {
        result.error = e.what();
    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

auto BlockInterpreter::has_result() const -> bool {
    return !result_.error.empty() || result_.block.get() != nullptr;
}

} // namespace mnesso::interpreters
