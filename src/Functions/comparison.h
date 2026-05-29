// src/Functions/comparison.h — Comparison function implementations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_function.h"
#include <cstdint>
#include <string>

namespace mnesso::functions {

// ── Eq — element-wise equality ──
class FunctionEq final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionEq>(); }
};

// ── Ne — element-wise not-equal ──
class FunctionNe final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionNe>(); }
};

// ── Gt — element-wise greater-than ──
class FunctionGt final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionGt>(); }
};

// ── Lt — element-wise less-than ──
class FunctionLt final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionLt>(); }
};

// ── Ge — element-wise greater-or-equal ──
class FunctionGe final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionGe>(); }
};

// ── Le — element-wise less-or-equal ──
class FunctionLe final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionLe>(); }
};

} // namespace mnesso::functions
