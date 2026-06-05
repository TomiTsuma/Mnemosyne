// tests/test_coordination.h — Unit tests for coordination layer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "coordination/coordination.h"
#include <catch2/catch_all.hpp>

// ── Coordination tests ──
TEST_CASE("Coordination creates cluster", "[coordination]") {
    auto coord = mnesso::coordination::Coordination::create(
        "test_cluster",
        {"node1:4311", "node2:4311", "node3:4311"},
        "node1:4311");

    REQUIRE(coord != nullptr);

    coord->start();
    REQUIRE(coord->is_running());
}
