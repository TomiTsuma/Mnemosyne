// src/AggregateFunctions/sum.cpp — SUM aggregate function implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/sum.h"
#include "DataTypes/data_type_number.h"
#include "Core/field.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <numeric>

namespace mnemo::aggregate_functions {

// ── SumState ──

void SumState::init() {
    value_ = 0.0;
    count_ = 0;
    has_value_ = false;
}

void SumState::reset() {
    value_ = 0.0;
    count_ = 0;
    has_value_ = false;
}

void SumState::add(const Field& field) {
    auto num = field.as_float64();
    if (num) {
        value_ += *num;
        count_++;
        has_value_ = true;
    }
}

void SumState::add(const std::vector<Field>& fields) {
    for (const auto& f : fields) {
        add(f);
    }
}

auto SumState::result() const -> Field {
    if (!has_value_) return Field{};
    return Field{value_};
}

auto SumState::result_string() const -> std::string {
    if (!has_value_) return "";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", value_);
    return std::string{buf};
}

bool SumState::has_result() const {
    return has_value_;
}

auto SumState::clone() const -> void* {
    auto* state = new SumState{};
    state->value_ = value_;
    state->count_ = count_;
    state->has_value_ = has_value_;
    return state;
}

// ── FunctionSum ──

auto FunctionSum::create(const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    auto func = std::make_shared<FunctionSum>();
    if (!arg_types.empty()) {
        func->arg_type_ = arg_types[0];
    }
    return func;
}

auto FunctionSum::name() const -> std::string {
    return "sum";
}

auto FunctionSum::arg_types() const -> std::vector<datatypes::DataTypePtr> {
    return {};
}

auto FunctionSum::result_type() const -> datatypes::DataTypePtr {
    return datatypes::make_data_type_float64();
}

void FunctionSum::init() {
    // State initialized as zeros in the arena
}

void FunctionSum::add(std::span<const uint8_t> state,
                      std::span<const Field> args,
                      Arena& arena) {
    (void)arena;
    if (!state.empty() && !args.empty()) {
        auto* sum_ptr = const_cast<double*>(reinterpret_cast<const double*>(state.data()));
        for (const auto& arg : args) {
            auto num = arg.as_float64();
            if (num) {
                *sum_ptr += *num;
            }
        }
    }
}

void FunctionSum::merge(std::span<const uint8_t> state,
                        std::span<const uint8_t> other_state,
                        Arena& arena) {
    (void)arena;
    if (!state.empty() && !other_state.empty()) {
        auto* dest_sum = const_cast<double*>(reinterpret_cast<const double*>(state.data()));
        const auto* src_sum = reinterpret_cast<const double*>(other_state.data());
        *dest_sum += *src_sum;
    }
}

void FunctionSum::finalize(std::span<const uint8_t> state, Arena& arena) {
    (void)state;
    (void)arena;
    // SUM just keeps running sum, nothing to finalize
}

auto FunctionSum::memory_usage() const -> size_t {
    return sizeof(double);
}

} // namespace mnemo::aggregate_functions