// src/AggregateFunctions/sum.h — SUM aggregate function
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_aggregate_function.h"
#include <string>
#include <memory>
#include <vector>

namespace mnesso::aggregate_functions {

class FunctionSum final : public IAggregateFunction {
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
    explicit FunctionSum(datatypes::DataTypePtr arg_type);
    datatypes::DataTypePtr arg_type_;
};

} // namespace mnesso::aggregate_functions
