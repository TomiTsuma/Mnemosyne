// src/Interpreters/interpreter_alter_view.h — ALTER VIEW / ALTER MATERIALIZED VIEW interpreters

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterAlterView {
public:
    /// ALTER VIEW <name> AS SELECT ...
    /// Replaces the definition only if the new one is valid, not circular, and leaves every
    /// view that reads this one still valid; otherwise the old definition is kept.
    static auto execute_view(Context& context, const parsers::QueryAST& query) -> core::Block;

    /// ALTER MATERIALIZED VIEW <name> SET REFRESH EVERY <n> <unit> | MANUAL
    /// Changes how the view is refreshed. The data is not refreshed by the ALTER itself.
    static auto execute_materialized_view(Context& context, const parsers::QueryAST& query)
        -> core::Block;
};

} // namespace mnemo::interpreters
