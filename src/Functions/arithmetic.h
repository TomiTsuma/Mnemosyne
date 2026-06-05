// src/Functions/arithmetic.h — Arithmetic function implementations
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_function.h"
#include <cstdint>
#include <string>

namespace mnemo::functions {

// ── Add — element-wise addition ──
class FunctionAdd final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionAdd>(); }

private:
    bool prepared_ = false;
};

// ── Sub — element-wise subtraction ──
class FunctionSub final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionSub>(); }

private:
    bool prepared_ = false;
};

// ── Mul — element-wise multiplication ──
class FunctionMul final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionMul>(); }

private:
    bool prepared_ = false;
};

// ── Div — element-wise integer division (floor) ──
class FunctionDiv final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionDiv>(); }

private:
    bool prepared_ = false;
};

// ── Mod — element-wise modulo ──
class FunctionMod final : public IFunction {
public:
    [[nodiscard]] auto info()  const -> FunctionInfo override;
    [[nodiscard]] auto execute(const core::Block& input) -> core::Block override;
    void  prepare(const std::vector<datatypes::DataTypePtr>& arg_types) override;
    [[nodiscard]] bool can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const override;

    static auto create() -> IFunctionPtr { return std::make_shared<FunctionMod>(); }

private:
    bool prepared_ = false;
};

} // namespace mnemo::functions
