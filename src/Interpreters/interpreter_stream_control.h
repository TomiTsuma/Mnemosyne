// src/Interpreters/interpreter_stream_control.h

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterStreamControl {
public:
    static auto execute_publish(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto execute_subscribe(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
