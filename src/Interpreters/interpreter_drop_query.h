// src/Interpreters/interpreter_drop_query.h — DROP / TRUNCATE / DETACH interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterDropQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;

private:
    static auto do_drop(Context& context, const parsers::QueryAST::Drop& drop) -> void;
    static auto do_drop_view(Context& context, const parsers::QueryAST::Drop& drop) -> void;
    static auto do_drop_materialized_view(Context& context, const parsers::QueryAST::Drop& drop)
        -> void;
    static auto do_truncate(Context& context, const parsers::QueryAST::Drop& drop) -> void;
    static auto do_detach(Context& context, const parsers::QueryAST::Drop& drop) -> void;
};

} // namespace mnemo::interpreters
