// src/AggregateFunctions/i_aggregate_function.cpp — IAggregateFunction interface implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/i_aggregate_function.h"
#include "Core/field.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::aggregate_functions {

// IAggregateFunction is mostly a header-only template interface.
// Concrete implementations are in sum.cpp, count.cpp, avg.cpp, min_max.cpp.

} // namespace mnesso::aggregate_functions