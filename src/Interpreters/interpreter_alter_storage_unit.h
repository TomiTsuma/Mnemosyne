// src/Interpreters/interpreter_alter_storage_unit.h — ALTER STORAGE_UNIT interpreter

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

// ALTER STORAGE_UNIT <name> SET RETENTION <n> DAYS | FOREVER.
// The policy is recorded and reported; nothing expires data yet (see StorageUnitEntry).
class InterpreterAlterStorageUnit {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
