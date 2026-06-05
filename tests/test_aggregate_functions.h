// tests/test_aggregate_functions.h — Unit tests for aggregate functions
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "AggregateFunctions/aggregate_function_factory.h"
#include "AggregateFunctions/sum.h"
#include "AggregateFunctions/count.h"
#include "AggregateFunctions/avg.h"
#include "AggregateFunctions/min_max.h"
#include "DataTypes/data_type_number.h"
#include <catch2/catch_all.hpp>

// ── Aggregate function tests ──
TEST_CASE("AggregateFunctionFactory singleton", "[aggregate]") {
    auto& factory = mnemo::aggregate_functions::AggregateFunctionFactory::instance();
    auto& factory2 = mnemo::aggregate_functions::AggregateFunctionFactory::instance();
    REQUIRE(&factory == &factory2);
}

TEST_CASE("SUM function registered", "[aggregate]") {
    auto& factory = mnemo::aggregate_functions::AggregateFunctionFactory::instance();
    auto func = factory.get("sum", {});
    REQUIRE(func != nullptr);
    REQUIRE(func->name() == "sum");
}

TEST_CASE("COUNT function registered", "[aggregate]") {
    auto& factory = mnemo::aggregate_functions::AggregateFunctionFactory::instance();
    auto func = factory.get("count", {});
    REQUIRE(func != nullptr);
    REQUIRE(func->name() == "count");
}
