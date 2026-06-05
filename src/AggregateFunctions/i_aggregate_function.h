// src/AggregateFunctions/i_aggregate_function.h — IAggregateFunction base interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <memory_resource>
#include <vector>
#include "Common/span_compat.h"
#include <cstdint>
#include "Core/field.h"
#include "DataTypes/data_type.h"

namespace mnemo::aggregate_functions {

using core::Field;

class IAggregateFunction;
using AggregateFunctionPtr = std::shared_ptr<IAggregateFunction>;

// ── Arena — short-lived memory pool for aggregate state ──
using Arena = std::pmr::monotonic_buffer_resource;

// ── IAggregateFunction — abstract base for all aggregate functions ──
// Each aggregate maintains state across a block of rows and produces
// a single result value at the end.
class IAggregateFunction {
public:
    virtual ~IAggregateFunction() = default;

    // Function identification
    [[nodiscard]] virtual auto name()  const -> std::string = 0;
    [[nodiscard]] virtual auto arg_types() const -> std::vector<datatypes::DataTypePtr> = 0;
    [[nodiscard]] virtual auto result_type() const -> datatypes::DataTypePtr = 0;

    // Initialization — called before processing begins
    virtual void init() = 0;

    // Process a single row — accumulate into state
    virtual void add(std::span<const uint8_t> state,
                     std::span<const Field> args,
                     Arena& arena) = 0;

    // Merge state from another aggregate (for parallel execution)
    virtual void merge(std::span<const uint8_t> state,
                       std::span<const uint8_t> other_state,
                       Arena& arena) = 0;

    // Finalize — compute result from accumulated state
    virtual void finalize(std::span<const uint8_t> state,
                          Arena& arena) = 0;

    // Memory estimate
    [[nodiscard]] virtual auto memory_usage() const -> size_t = 0;
};

} // namespace mnemo::aggregate_functions
