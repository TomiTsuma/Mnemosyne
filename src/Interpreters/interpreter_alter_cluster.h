// src/Interpreters/interpreter_alter_cluster.h — ALTER CLUSTER interpreter

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

// Handles ALTER CLUSTER <name> ADD/REMOVE NODE <node> — cluster membership changes.
class InterpreterAlterCluster {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
