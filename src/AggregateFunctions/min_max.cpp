// src/AggregateFunctions/min_max.cpp — MIN/MAX aggregate function implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/min_max.h"
#include "DataTypes/data_type_number.h"
#include "Core/field.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <limits>

namespace mnesso::aggregate_functions {

// ── MinState ──

void MinState::init() {
    has_value_ = false;
}

void MinState::reset() {
    has_value_ = false;
}

void MinState::add(const Field& field) {
    if (field.is_null()) return;
    auto num = field.as_float64();
    if (num) {
        if (!has_value_ || *num < value_) {
            value_ = *num;
            has_value_ = true;
        }
    } else {
        auto str = field.as_string();
        if (str) {
            if (!has_value_ || *str < str_value_) {
                str_value_ = *str;
                has_value_ = true;
            }
        }
    }
}

void MinState::add(const std::vector<Field>& fields) {
    for (const auto& f : fields) {
        add(f);
    }
}

auto MinState::result() const -> Field {
    if (!has_value_) return Field{};
    return Field{value_};
}

auto MinState::result_string() const -> std::string {
    if (!has_value_) return "";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", value_);
    return std::string{buf};
}

bool MinState::has_result() const {
    return has_value_;
}

auto MinState::clone() const -> void* {
    auto* state = new MinState{};
    state->value_ = value_;
    state->has_value_ = has_value_;
    state->str_value_ = str_value_;
    return state;
}

// ── FunctionMin ──

auto FunctionMin::create(const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    return std::make_shared<FunctionMin>();
}

auto FunctionMin::name() const -> std::string {
    return "min";
}

auto FunctionMin::arg_types() const -> std::vector<datatypes::DataTypePtr> {
    return {};
}

auto FunctionMin::result_type() const -> datatypes::DataTypePtr {
    return datatypes::make_data_type_float64();
}

void FunctionMin::init() {
    // State initialized as needed in arena
}

void FunctionMin::add(std::span<const uint8_t> state,
                      std::span<const Field> args,
                      Arena& arena) {
    (void)arena;
    if (!state.empty() && !args.empty()) {
        struct State {
            double value;
            bool has_value;
        };
        auto* min_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        for (const auto& arg : args) {
            if (arg.is_null()) continue;
            auto num = arg.as_float64();
            if (num) {
                if (!min_state->has_value || *num < min_state->value) {
                    min_state->value = *num;
                    min_state->has_value = true;
                }
            }
        }
    }
}

void FunctionMin::merge(std::span<const uint8_t> state,
                        std::span<const uint8_t> other_state,
                        Arena& arena) {
    (void)arena;
    if (!state.empty() && !other_state.empty()) {
        struct State {
            double value;
            bool has_value;
        };
        auto* dest_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        const auto* src_state = reinterpret_cast<const State*>(other_state.data());
        if (!dest_state->has_value || (src_state->has_value && src_state->value < dest_state->value)) {
            dest_state->value = src_state->value;
            dest_state->has_value = true;
        }
    }
}

void FunctionMin::finalize(std::span<const uint8_t> state, Arena& arena) {
    (void)state;
    (void)arena;
}

auto FunctionMin::memory_usage() const -> size_t {
    return sizeof(double) + sizeof(bool);
}

// ── FunctionMax ──

auto FunctionMax::create(const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    return std::make_shared<FunctionMax>();
}

auto FunctionMax::name() const -> std::string {
    return "max";
}

auto FunctionMax::arg_types() const -> std::vector<datatypes::DataTypePtr> {
    return {};
}

auto FunctionMax::result_type() const -> datatypes::DataTypePtr {
    return datatypes::make_data_type_float64();
}

void FunctionMax::init() {
    // State initialized as needed in arena
}

void FunctionMax::add(std::span<const uint8_t> state,
                      std::span<const Field> args,
                      Arena& arena) {
    (void)arena;
    if (!state.empty() && !args.empty()) {
        struct State {
            double value;
            bool has_value;
        };
        auto* max_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        for (const auto& arg : args) {
            if (arg.is_null()) continue;
            auto num = arg.as_float64();
            if (num) {
                if (!max_state->has_value || *num > max_state->value) {
                    max_state->value = *num;
                    max_state->has_value = true;
                }
            }
        }
    }
}

void FunctionMax::merge(std::span<const uint8_t> state,
                        std::span<const uint8_t> other_state,
                        Arena& arena) {
    (void)arena;
    if (!state.empty() && !other_state.empty()) {
        struct State {
            double value;
            bool has_value;
        };
        auto* dest_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        const auto* src_state = reinterpret_cast<const State*>(other_state.data());
        if (!dest_state->has_value || (src_state->has_value && src_state->value > dest_state->value)) {
            dest_state->value = src_state->value;
            dest_state->has_value = true;
        }
    }
}

void FunctionMax::finalize(std::span<const uint8_t> state, Arena& arena) {
    (void)state;
    (void)arena;
}

auto FunctionMax::memory_usage() const -> size_t {
    return sizeof(double) + sizeof(bool);
}

} // namespace mnesso::aggregate_functions