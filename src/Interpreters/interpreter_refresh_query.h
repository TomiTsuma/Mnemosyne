// src/Interpreters/interpreter_refresh_query.h — REFRESH MATERIALIZED VIEW interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterRefreshQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
