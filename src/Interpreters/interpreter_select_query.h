// src/Interpreters/interpreter_select_query.h — AST-driven SELECT execution
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Interpreters/context.h"
#include "Core/block.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterSelectQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
