// src/Interpreters/interpreter_insert_query.h — INSERT INTO interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterInsertQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;

private:
    static auto build_insert_block(
        const storages::IStorage& storage,
        const std::vector<std::string>& columns,
        const std::vector<std::vector<std::string>>& values) -> core::Block;
};

} // namespace mnemo::interpreters
