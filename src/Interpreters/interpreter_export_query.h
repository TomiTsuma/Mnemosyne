// src/Interpreters/interpreter_export_query.h — EXPORT TABLE ... TO CONNECTOR interpreter

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

// EXPORT TABLE <table> TO CONNECTOR <connector>[.<resource>] [MODE APPEND | OVERWRITE]
//
// Reads the table (or view, or materialized view) and hands every row to the connector driver in
// one write. Returns one row: connector, resource, mode, rows. MODE defaults to APPEND, because
// OVERWRITE replaces data in the external system. The resource defaults to the table's name.
class InterpreterExportQuery {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
