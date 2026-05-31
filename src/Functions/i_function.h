// src/Functions/i_function.h — IFunction: scalar function interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>
#include "DataTypes/data_type.h"
#include "Core/block.h"

namespace mnesso::functions {

class IFunction;
using IFunctionPtr = std::shared_ptr<IFunction>;

// ── Function metadata ──
struct FunctionInfo {
    std::string    name;
    std::string    description;
    size_t         arg_count;
    bool           is_deterministic;
    bool           is_nullable;
};

// ── IFunction — abstract scalar function ──
// Each function operates on Blocks and returns a Block with the result.
class IFunction {
public:
    virtual ~IFunction() = default;

    // Function info
    [[nodiscard]] virtual auto info() const -> FunctionInfo = 0;

    // Execute on input block — returns a new block with computed column
    [[nodiscard]] virtual auto execute(const core::Block& input) -> core::Block = 0;

    // Prepare for batch execution — called once per query
    virtual void prepare(const std::vector<datatypes::DataTypePtr>& arg_types) = 0;

    // Check if this function supports the given types
    [[nodiscard]] virtual bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const = 0;
};

} // namespace mnesso::functions
