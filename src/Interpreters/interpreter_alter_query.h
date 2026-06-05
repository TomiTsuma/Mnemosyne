// src/Interpreters/interpreter_alter_query.h — ALTER TABLE interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterAlterQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;

private:
    static auto add_column(storages::IStorage& storage,
                           const std::string& name,
                           const std::string& type_name) -> void;
    static auto drop_column(storages::IStorage& storage, const std::string& name) -> void;
    static auto modify_column(storages::IStorage& storage,
                              const std::string& name,
                              const std::string& type_name) -> void;
};

} // namespace mnemo::interpreters
