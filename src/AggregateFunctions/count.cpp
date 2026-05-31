// src/AggregateFunctions/count.cpp — COUNT aggregate function implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "AggregateFunctions/count.h"
#include "DataTypes/data_type_number.h"
#include "Core/field.h"
#include "Common/exceptions.h"

namespace mnesso::aggregate_functions {

// ── CountState ──

void CountState::init() {
    count_ = 0;
}

void CountState::reset() {
    count_ = 0;
}

void CountState::add(const Field& field) {
    if (!field.is_null()) {
        count_++;
    }
}

void CountState::add(const std::vector<Field>& fields) {
    for (const auto& f : fields) {
        add(f);
    }
}

auto CountState::result() const -> Field {
    return Field{static_cast<int64_t>(count_)};
}

auto CountState::result_string() const -> std::string {
    return std::to_string(count_);
}

bool CountState::has_result() const {
    return true;
}

auto CountState::clone() const -> void* {
    auto* state = new CountState{};
    state->count_ = count_;
    return state;
}

// ── FunctionCount ──

auto FunctionCount::create(const std::vector<datatypes::DataTypePtr>& arg_types)
    -> AggregateFunctionPtr {
    return std::make_shared<FunctionCount>();
}

auto FunctionCount::name() const -> std::string {
    return "count";
}

auto FunctionCount::arg_types() const -> std::vector<datatypes::DataTypePtr> {
    // COUNT accepts any argument type
    return {};
}

auto FunctionCount::result_type() const -> datatypes::DataTypePtr {
    // COUNT returns int64
    return datatypes::make_data_type_int64();
}

void FunctionCount::init() {
    count_ = 0;
}

void FunctionCount::add(std::span<const uint8_t> state,
                        std::span<const Field> args,
                        Arena& arena) {
    (void)arena;
    if (!state.empty() && args.size() > 0) {
        auto* count_ptr = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(state.data()));
        bool has_non_null = false;
        for (const auto& arg : args) {
            if (!arg.is_null()) {
                has_non_null = true;
                break;
            }
        }
        if (has_non_null) {
            (*count_ptr)++;
        }
    }
}

void FunctionCount::merge(std::span<const uint8_t> state,
                          std::span<const uint8_t> other_state,
                          Arena& arena) {
    (void)arena;
    if (!state.empty() && !other_state.empty()) {
        auto* dest_count = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(state.data()));
        const auto* src_count = reinterpret_cast<const uint64_t*>(other_state.data());
        *dest_count += *src_count;
    }
}

void FunctionCount::finalize(std::span<const uint8_t> state, Arena& arena) {
    (void)state;
    (void)arena;
    // COUNT just keeps running count, nothing to finalize
}

auto FunctionCount::memory_usage() const -> size_t {
    return sizeof(uint64_t);
}

} // namespace mnesso::aggregate_functions