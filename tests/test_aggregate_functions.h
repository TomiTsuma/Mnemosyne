// tests/test_aggregate_functions.h — Unit tests for aggregate functions
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "aggregate_functions/aggregate_function_factory.h"
#include "aggregate_functions/sum.h"
#include "aggregate_functions/count.h"
#include "aggregate_functions/avg.h"
#include "aggregate_functions/min_max.h"
#include "data_types/data_type_number.h"
#include <catch2/catch_all.hpp>

// ── Aggregate function tests ──
TEST_CASE("AggregateFunctionFactory singleton", "[aggregate]") {
    auto& factory = mnesso::aggregate_functions::AggregateFunctionFactory::instance();
    auto& factory2 = mnesso::aggregate_functions::AggregateFunctionFactory::instance();
    REQUIRE(&factory == &factory2);
}

TEST_CASE("SUM function registered", "[aggregate]") {
    auto& factory = mnesso::aggregate_functions::AggregateFunctionFactory::instance();
    auto func = factory.get("sum", {});
    REQUIRE(func != nullptr);
    REQUIRE(func->name() == "sum");
}

TEST_CASE("COUNT function registered", "[aggregate]") {
    auto& factory = mnesso::aggregate_functions::AggregateFunctionFactory::instance();
    auto func = factory.get("count", {});
    REQUIRE(func != nullptr);
    REQUIRE(func->name() == "count");
}
