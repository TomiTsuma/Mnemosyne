// src/Functions/function_factory.cpp — Registry of all registered functions
// Mnemosyne: A column-oriented analytical DBMS

#include "Functions/function_factory.h"
#include "Functions/arithmetic.h"
#include "Functions/comparison.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnesso::functions {

// ── FunctionFactory ──

FunctionFactory& FunctionFactory::instance() {
    static FunctionFactory inst;
    return inst;
}

void FunctionFactory::register_function(std::string name,
                                         std::function<IFunctionPtr()> creator) {
    registry_[std::move(name)] = std::move(creator);
}

auto FunctionFactory::get(std::string_view name) -> IFunctionPtr {
    auto it = registry_.find(std::string{name});
    if (it == registry_.end()) return nullptr;
    return it->second();
}

auto FunctionFactory::names() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(registry_.size());
    for (const auto& [name, _] : registry_) {
        result.push_back(name);
    }
    return result;
}

bool FunctionFactory::has(std::string_view name) const {
    return registry_.find(std::string{name}) != registry_.end();
}

// ── Global function lookup ──

static FunctionFactory& get_factory() {
    auto& factory = FunctionFactory::instance();

    static bool registered = false;
    if (!registered) {
        // Arithmetic functions
        factory.register_function("add", FunctionAdd::create);
        factory.register_function("sub", FunctionSub::create);
        factory.register_function("mul", FunctionMul::create);
        factory.register_function("div", FunctionDiv::create);
        factory.register_function("mod", FunctionMod::create);

        // Comparison functions
        factory.register_function("eq", FunctionEq::create);
        factory.register_function("ne", FunctionNe::create);
        factory.register_function("gt", FunctionGt::create);
        factory.register_function("lt", FunctionLt::create);
        factory.register_function("ge", FunctionGe::create);
        factory.register_function("le", FunctionLe::create);

        registered = true;
    }
    return factory;
}

IFunctionPtr get_function(std::string_view name) {
    auto& factory = get_factory();
    auto func = factory.get(name);
    if (!func) {
        throw common::Exception{
            "get_function: unknown function '" + std::string{name} + "'",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return func;
}

} // namespace mnesso::functions