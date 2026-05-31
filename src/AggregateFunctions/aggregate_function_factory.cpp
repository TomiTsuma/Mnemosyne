// src/AggregateFunctions/aggregate_function_factory.cpp — Registry of all registered aggregate functions
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/aggregate_function_factory.h"
#include "AggregateFunctions/sum.h"
#include "AggregateFunctions/count.h"
#include "AggregateFunctions/avg.h"
#include "AggregateFunctions/min_max.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::aggregate_functions {

// ── AggregateFunctionFactory ──

auto AggregateFunctionFactory::instance() -> AggregateFunctionFactory& {
    static AggregateFunctionFactory inst;
    return inst;
}

void AggregateFunctionFactory::register_function(std::string name,
                                                  std::function<AggregateFunctionPtr(
                                                      const std::vector<datatypes::DataTypePtr>&)> creator) {
    registry_[std::move(name)] = std::move(creator);
}

auto AggregateFunctionFactory::get(std::string_view name,
                                   const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    auto it = registry_.find(std::string{name});
    if (it == registry_.end()) return nullptr;
    return it->second(arg_types);
}

auto AggregateFunctionFactory::names() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(registry_.size());
    for (const auto& [name, _] : registry_) {
        result.push_back(name);
    }
    return result;
}

bool AggregateFunctionFactory::has(std::string_view name) const {
    return registry_.find(std::string{name}) != registry_.end();
}

// ── Global function lookup ──

static AggregateFunctionFactory& get_factory() {
    auto& factory = AggregateFunctionFactory::instance();

    static bool registered = false;
    if (!registered) {
        factory.register_function("sum", FunctionSum::create);
        factory.register_function("count", FunctionCount::create);
        factory.register_function("avg", FunctionAvg::create);
        factory.register_function("min", FunctionMin::create);
        factory.register_function("max", FunctionMax::create);

        registered = true;
    }
    return factory;
}

AggregateFunctionPtr get_aggregate_function(
    std::string_view name,
    const std::vector<datatypes::DataTypePtr>& arg_types) {
    auto& factory = get_factory();
    auto func = factory.get(name, arg_types);
    if (!func) {
        throw common::Exception{
            "get_aggregate_function: unknown aggregate function '" + std::string{name} + "'",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return func;
}

} // namespace mnesso::aggregate_functions