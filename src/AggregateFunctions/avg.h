// src/AggregateFunctions/avg.h — AVG aggregate function
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_aggregate_function.h"
#include <string>
#include <memory>
#include <cstdint>

namespace mnesso::aggregate_functions {

class FunctionAvg final : public IAggregateFunction {
public:
    static auto create(const std::vector<datatypes::DataTypePtr>& arg_types)
        -> AggregateFunctionPtr;

    [[nodiscard]] auto name()  const -> std::string override;
    [[nodiscard]] auto arg_types() const -> std::vector<datatypes::DataTypePtr> override;
    [[nodiscard]] auto result_type() const -> datatypes::DataTypePtr override;

    void init() override;
    void add(std::span<const uint8_t> state,
             std::span<const Field> args,
             Arena& arena) override;
    void merge(std::span<const uint8_t> state,
               std::span<const uint8_t> other_state,
               Arena& arena) override;
    void finalize(std::span<const uint8_t> state,
                  Arena& arena) override;
    [[nodiscard]] auto memory_usage() const -> size_t override;

private:
    uint64_t count_   = 0;
    double   sum_     = 0.0;
};

} // namespace mnesso::aggregate_functions
