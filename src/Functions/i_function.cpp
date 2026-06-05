// src/Functions/i_function.cpp — IFunction interface implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Functions/i_function.h"
#include "Functions/arithmetic.h"
#include "Functions/comparison.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::functions {

// IFunction is mostly a header-only template interface.
// Concrete implementations are in arithmetic.cpp and comparison.cpp.

} // namespace mnemo::functions