// tests/test_planner.h — Unit tests for query planner
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "planner/planner.h"
#include "planner/execution_plan.h"
#include "analyzer/query_tree.h"
#include <catch2/catch_all.hpp>

// ── Planner tests ──
TEST_CASE("Planner creates plan for SELECT", "[planner]") {
    mnesso::interpreters::Context context;
    mnesso::planner::Planner planner(context);

    // Create a mock select node
    auto select_node = std::make_shared<mnesso::analyzer::SelectNode>();
    auto plan = planner.plan(select_node);

    REQUIRE(plan != nullptr);
    REQUIRE(plan->root != nullptr);
}

TEST_CASE("Planner estimates costs", "[planner]") {
    mnesso::interpreters::Context context;
    mnesso::planner::Planner planner(context);

    auto select_node = std::make_shared<mnesso::analyzer::SelectNode>();
    auto plan = planner.plan(select_node);

    auto cost = planner.estimate_cost(plan);
    REQUIRE(cost >= 0.0);
}
