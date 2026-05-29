// src/Functions/function_factory.h — Registry of all registered scalar functions
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_function.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace mnesso::functions {

// ── FunctionFactory — singleton registry ──
class FunctionFactory {
public:
    static auto& instance();

    void register_function(std::string name, std::function<IFunctionPtr()> creator);

    [[nodiscard]] auto get(std::string_view name) -> IFunctionPtr;
    [[nodiscard]] TypeId resolve(std::string_view name) const;
    [[nodiscard]] auto names() const -> std::vector<std::string>;
    [[nodiscard]] bool has(std::string_view name) const;

private:
    FunctionFactory() = default;
    std::unordered_map<std::string, std::function<IFunctionPtr()>> registry_;
};

// ── Builtin function names ──
namespace Builtin {
    // Arithmetic
    inline constexpr auto ADD    = "add";
    inline constexpr auto SUB    = "sub";
    inline constexpr auto MUL    = "mul";
    inline constexpr auto DIV    = "div";
    inline constexpr auto MOD    = "mod";
    inline constexpr auto NEG    = "neg";

    // Comparison
    inline constexpr auto EQ     = "eq";
    inline constexpr auto NE     = "ne";
    inline constexpr auto GT     = "gt";
    inline constexpr auto LT     = "lt";
    inline constexpr auto GE     = "ge";
    inline constexpr auto LE     = "le";

    // TODO: more categories — math, string, date, aggregate
}

} // namespace mnesso::functions
