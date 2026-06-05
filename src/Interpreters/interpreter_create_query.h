// src/Interpreters/interpreter_create_query.h — CREATE TABLE / DATABASE interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterCreateQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;

private:
    static auto do_create_table(Context& context, const parsers::QueryAST::Create& create)
        -> void;
    static auto do_create_database(Context& context, const parsers::QueryAST::Create& create)
        -> void;
};

} // namespace mnemo::interpreters
