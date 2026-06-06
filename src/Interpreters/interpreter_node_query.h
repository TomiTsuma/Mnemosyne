// src/Interpreters/interpreter_node_query.h — REGISTER / DRAIN / REMOVE NODE

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterNodeQuery {
public:
    static auto execute_register(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto execute_drain(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto execute_remove(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
