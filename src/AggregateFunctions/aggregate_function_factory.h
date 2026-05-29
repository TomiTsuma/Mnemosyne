// src/AggregateFunctions/aggregate_function_factory.h — Registry for aggregate functions
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_aggregate_function.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace mnesso::aggregate_functions {

// ── Factory — registry of all aggregate functions ──
class AggregateFunctionFactory {
public:
    static auto& instance();

    void register_function(std::string name,
                           std::function<AggregateFunctionPtr(
                               const std::vector<datatypes::DataTypePtr>&)> creator);

    [[nodiscard]] auto get(std::string_view name,
                           const std::vector<datatypes::DataTypePtr>& arg_types)
        -> AggregateFunctionPtr;

    [[nodiscard]] auto names() const -> std::vector<std::string>;
    [[nodiscard]] bool has(std::string_view name) const;

private:
    AggregateFunctionFactory() = default;
    std::unordered_map<std::string,
        std::function<AggregateFunctionPtr(
            const std::vector<datatypes::DataTypePtr>&)>> registry_;
};

} // namespace mnesso::aggregate_functions
