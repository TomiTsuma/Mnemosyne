// src/AggregateFunctions/avg.cpp — AVG aggregate function implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/avg.h"
#include "DataTypes/data_type_number.h"
#include "Core/field.h"
#include "Common/exceptions.h"

namespace mnemo::aggregate_functions {

// ── AvgState ──

void AvgState::init() {
    sum_ = 0.0;
    count_ = 0;
    has_value_ = false;
}

void AvgState::reset() {
    sum_ = 0.0;
    count_ = 0;
    has_value_ = false;
}

void AvgState::add(const Field& field) {
    auto num = field.as_float64();
    if (num) {
        sum_ += *num;
        count_++;
        has_value_ = true;
    }
}

void AvgState::add(const std::vector<Field>& fields) {
    for (const auto& f : fields) {
        add(f);
    }
}

auto AvgState::result() const -> Field {
    if (!has_value_) return Field{};
    return Field{sum_ / static_cast<double>(count_)};
}

auto AvgState::result_string() const -> std::string {
    if (!has_value_) return "";
    double avg = sum_ / static_cast<double>(count_);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", avg);
    return std::string{buf};
}

bool AvgState::has_result() const {
    return has_value_;
}

auto AvgState::clone() const -> void* {
    auto* state = new AvgState{};
    state->sum_ = sum_;
    state->count_ = count_;
    state->has_value_ = has_value_;
    return state;
}

// ── FunctionAvg ──

auto FunctionAvg::create(const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    return std::make_shared<FunctionAvg>();
}

auto FunctionAvg::name() const -> std::string {
    return "avg";
}

auto FunctionAvg::arg_types() const -> std::vector<datatypes::DataTypePtr> {
    return {};
}

auto FunctionAvg::result_type() const -> datatypes::DataTypePtr {
    return datatypes::make_data_type_float64();
}

void FunctionAvg::init() {
    // State initialized as zeros in the arena
}

void FunctionAvg::add(std::span<const uint8_t> state,
                      std::span<const Field> args,
                      Arena& arena) {
    (void)arena;
    if (!state.empty() && !args.empty()) {
        struct State {
            double sum;
            int64_t count;
        };
        auto* avg_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        for (const auto& arg : args) {
            auto num = arg.as_float64();
            if (num) {
                avg_state->sum += *num;
                avg_state->count++;
            }
        }
    }
}

void FunctionAvg::merge(std::span<const uint8_t> state,
                        std::span<const uint8_t> other_state,
                        Arena& arena) {
    (void)arena;
    if (!state.empty() && !other_state.empty()) {
        struct State {
            double sum;
            int64_t count;
        };
        auto* dest_state = const_cast<State*>(reinterpret_cast<const State*>(state.data()));
        const auto* src_state = reinterpret_cast<const State*>(other_state.data());
        dest_state->sum += src_state->sum;
        dest_state->count += src_state->count;
    }
}

void FunctionAvg::finalize(std::span<const uint8_t> state, Arena& arena) {
    (void)state;
    (void)arena;
    // AVG just accumulates, nothing to finalize
}

auto FunctionAvg::memory_usage() const -> size_t {
    return sizeof(double) + sizeof(int64_t);
}

} // namespace mnemo::aggregate_functions