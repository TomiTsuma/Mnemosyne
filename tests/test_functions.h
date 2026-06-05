// tests/test_functions.h — Unit tests for scalar functions
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "functions/function_factory.h"
#include "functions/arithmetic.h"
#include "functions/comparison.h"
#include <catch2/catch_all.hpp>

// ── Function tests ──
TEST_CASE("Add function", "[function]") {
    auto& factory = mnemo::functions::FunctionFactory::instance();
    auto func = factory.get("add");
    REQUIRE(func != nullptr);
    REQUIRE(func->info().name == "add");
    REQUIRE(func->info().arg_count == 2);
}

TEST_CASE("Comparison functions", "[function]") {
    auto& factory = mnemo::functions::FunctionFactory::instance();

    REQUIRE(factory.get("eq") != nullptr);
    REQUIRE(factory.get("ne") != nullptr);
    REQUIRE(factory.get("gt") != nullptr);
    REQUIRE(factory.get("lt") != nullptr);
    REQUIRE(factory.get("ge") != nullptr);
    REQUIRE(factory.get("le") != nullptr);
}

TEST_CASE("Function names", "[function]") {
    auto& factory = mnemo::functions::FunctionFactory::instance();
    auto names = factory.names();

    REQUIRE(names.size() >= 6);
    REQUIRE(std::find(names.begin(), names.end(), "add") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "sub") != names.end());
}
