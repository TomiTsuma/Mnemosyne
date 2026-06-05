// src/Functions/function_factory.cpp — Registry of all registered functions
// Mnemosyne: A column-oriented analytical DBMS

#include "Functions/function_factory.h"
#include "Functions/arithmetic.h"
#include "Functions/comparison.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::functions {

// ── FunctionFactory ──

FunctionFactory& FunctionFactory::instance() {
    static FunctionFactory inst;
    static bool registered = []() {
        // Arithmetic functions
        inst.register_function("add", FunctionAdd::create);
        inst.register_function("sub", FunctionSub::create);
        inst.register_function("mul", FunctionMul::create);
        inst.register_function("div", FunctionDiv::create);
        inst.register_function("mod", FunctionMod::create);

        // Comparison functions
        inst.register_function("eq", FunctionEq::create);
        inst.register_function("ne", FunctionNe::create);
        inst.register_function("gt", FunctionGt::create);
        inst.register_function("lt", FunctionLt::create);
        inst.register_function("ge", FunctionGe::create);
        inst.register_function("le", FunctionLe::create);
        return true;
    }();
    (void)registered;
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
    return FunctionFactory::instance();
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

} // namespace mnemo::functions