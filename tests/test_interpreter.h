// tests/test_interpreter.h — Unit tests for query interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "interpreters/interpreter.h"
#include "interpreters/query_executor.h"
#include "planner/execution_plan.h"
#include <catch2/catch_all.hpp>

// ── Interpreter tests ──
TEST_CASE("Interpreter executes SELECT", "[interpreter]") {
    mnemo::interpreters::Context context;

    auto plan = std::make_shared<mnemo::planner::ExecutionPlan>(0, "test");
    auto interpreter = mnemo::interpreters::InterpreterFactory::create_select(plan, context);

    auto result = interpreter->execute();
    REQUIRE(result.block != nullptr);
}

TEST_CASE("Interpreter handles errors", "[interpreter]") {
    mnemo::interpreters::Context context;

    auto plan = std::make_shared<mnemo::planner::ExecutionPlan>(0, "test");
    auto interpreter = mnemo::interpreters::InterpreterFactory::create_select(plan, context);

    auto result = interpreter->execute();
    REQUIRE(result.block != nullptr);
}
