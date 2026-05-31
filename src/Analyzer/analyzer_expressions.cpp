// src/Analyzer/analyzer_expressions.cpp — Expression analysis utilities
// Mnemosyne: A column-oriented analytical DBMS

#include "Analyzer/analyzer.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::analyzer {

// Expression analysis is mostly implemented in analyzer.cpp
// This file provides additional helper functions.

bool is_aggregate_function(const std::string& name) {
    static const std::unordered_set<std::string> agg_funcs = {
        "sum", "count", "avg", "min", "max"
    };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return agg_funcs.count(lower) > 0;
}

bool is_column_in_group_by(const std::string& col,
                           const std::vector<ColumnRef>& group_by) {
    for (const auto& g : group_by) {
        if (g.column == col) return true;
    }
    return false;
}

bool is_column_in_select(const std::string& col,
                         const std::vector<ColumnRef>& select_cols) {
    for (const auto& s : select_cols) {
        if (s.column == col) return true;
    }
    return false;
}

} // namespace mnesso::analyzer