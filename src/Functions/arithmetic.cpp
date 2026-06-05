// src/Functions/arithmetic.cpp — Arithmetic function implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "Functions/arithmetic.h"
#include "Core/field.h"
#include "Core/block.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mnemo::functions {

namespace {

auto field_as_double(const core::Field& field) -> double {
    if (auto fv = field.as_float64()) {
        return *fv;
    }
    if (auto iv = field.as_int64()) {
        return static_cast<double>(*iv);
    }
    return 0.0;
}

} // namespace

// ── FunctionAdd ──

auto FunctionAdd::info() const -> FunctionInfo {
    return {"add", "Addition of two values", 2, true, true};
}

auto FunctionAdd::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionAdd::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<double>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = field_as_double(col1->get_at(i)) + field_as_double(col2->get_at(i));
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionAdd::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    prepared_ = true;
    (void)arg_types;
}

bool FunctionAdd::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionSub ──

auto FunctionSub::info() const -> FunctionInfo {
    return {"sub", "Subtraction of two values", 2, true, true};
}

auto FunctionSub::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionSub::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<double>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = field_as_double(col1->get_at(i)) - field_as_double(col2->get_at(i));
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionSub::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    prepared_ = true;
    (void)arg_types;
}

bool FunctionSub::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionMul ──

auto FunctionMul::info() const -> FunctionInfo {
    return {"mul", "Multiplication of two values", 2, true, true};
}

auto FunctionMul::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionMul::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<double>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = field_as_double(col1->get_at(i)) * field_as_double(col2->get_at(i));
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionMul::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    prepared_ = true;
    (void)arg_types;
}

bool FunctionMul::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionDiv ──

auto FunctionDiv::info() const -> FunctionInfo {
    return {"div", "Division of two values", 2, true, true};
}

auto FunctionDiv::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionDiv::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<double>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        const double divisor = field_as_double(col2->get_at(i));
        out[i] = divisor != 0.0 ? field_as_double(col1->get_at(i)) / divisor : 0.0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionDiv::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    prepared_ = true;
    (void)arg_types;
}

bool FunctionDiv::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionMod ──

auto FunctionMod::info() const -> FunctionInfo {
    return {"mod", "Modulo of two values", 2, true, true};
}

auto FunctionMod::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionMod::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<double>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        const double divisor = field_as_double(col2->get_at(i));
        out[i] = divisor != 0.0 ? std::fmod(field_as_double(col1->get_at(i)), divisor) : 0.0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionMod::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    prepared_ = true;
    (void)arg_types;
}

bool FunctionMod::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

} // namespace mnemo::functions