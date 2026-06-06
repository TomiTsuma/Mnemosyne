// src/Interpreters/interpreter_run_pipeline.h

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterRunPipeline {
public:
    static auto execute_run(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto execute_pause(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto execute_resume(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
