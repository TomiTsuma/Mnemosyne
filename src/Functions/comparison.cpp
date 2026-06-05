// src/Functions/comparison.cpp — Comparison function implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "Functions/comparison.h"
#include "Core/block.h"
#include "Core/field.h"
#include "Columns/column_vector.h"
#include "DataTypes/data_type.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::functions {

// ── FunctionEq ──

auto FunctionEq::info() const -> FunctionInfo {
    return {"eq", "Element-wise equality", 2, true, true};
}

auto FunctionEq::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionEq::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = (col1->get(i) == col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionEq::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionEq::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    return arg_types[0]->id() == arg_types[1]->id();
}

// ── FunctionNe ──

auto FunctionNe::info() const -> FunctionInfo {
    return {"ne", "Element-wise not-equal", 2, true, true};
}

auto FunctionNe::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionNe::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = !(col1->get(i) == col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionNe::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionNe::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    return arg_types[0]->id() == arg_types[1]->id();
}

// ── FunctionGt ──

auto FunctionGt::info() const -> FunctionInfo {
    return {"gt", "Element-wise greater-than", 2, true, true};
}

auto FunctionGt::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionGt::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = (col1->get(i) > col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionGt::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionGt::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionLt ──

auto FunctionLt::info() const -> FunctionInfo {
    return {"lt", "Element-wise less-than", 2, true, true};
}

auto FunctionLt::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionLt::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = (col1->get(i) < col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionLt::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionLt::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionGe ──

auto FunctionGe::info() const -> FunctionInfo {
    return {"ge", "Element-wise greater-or-equal", 2, true, true};
}

auto FunctionGe::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionGe::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = (col1->get(i) >= col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionGe::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionGe::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

// ── FunctionLe ──

auto FunctionLe::info() const -> FunctionInfo {
    return {"le", "Element-wise less-or-equal", 2, true, true};
}

auto FunctionLe::execute(const core::Block& input) -> core::Block {
    if (input.column_count() < 2) {
        throw common::Exception{
            "FunctionLe::execute: requires 2 input columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto col1 = input.get_column_by_index(0);
    auto col2 = input.get_column_by_index(1);
    size_t n = col1->size();

    auto result = std::make_shared<columns::ColumnVector<uint8_t>>();
    result->resize(n);
    auto out = result->get_mutable_span();

    for (size_t i = 0; i < n; ++i) {
        out[i] = (col1->get(i) <= col2->get(i)) ? 1 : 0;
    }

    core::Block block;
    block.add_column("result", result);
    return block;
}

void FunctionLe::prepare(const std::vector<datatypes::DataTypePtr>& arg_types) {
    (void)arg_types;
}

bool FunctionLe::can_execute(const std::vector<datatypes::DataTypePtr>& arg_types) const {
    if (arg_types.size() != 2) return false;
    auto id1 = arg_types[0]->id();
    auto id2 = arg_types[1]->id();
    return (id1 >= datatypes::TypeId::UInt8 && id1 <= datatypes::TypeId::Float64) &&
           (id2 >= datatypes::TypeId::UInt8 && id2 <= datatypes::TypeId::Float64);
}

} // namespace mnemo::functions