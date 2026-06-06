// src/Databases/view_catalog.h — View and materialized view catalog types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Parsers/ast.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mnemo::databases {

struct ViewEntry {
    std::string name;
    parsers::QueryAST::Select definition;
    std::vector<std::string> dependencies;
    std::string status = "ACTIVE";
};

struct MaterializedViewEntry {
    std::string name;
    parsers::QueryAST::Select definition;
    std::string backing_table;
    std::vector<std::string> dependencies;
    std::string status = "ACTIVE";
};

/// Collect relation names referenced by a SELECT definition.
auto extract_dependencies(const parsers::QueryAST::Select& select)
    -> std::vector<std::string>;

} // namespace mnemo::databases
