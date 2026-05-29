// tests/test_coordination.h — Unit tests for coordination layer
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "coordination/coordination.h"
#include <catch2/catch_all.hpp>

// ── Coordination tests ──
TEST_CASE("Coordination creates cluster", "[coordination]") {
    auto coord = mnesso::coordination::Coordination::create(
        "test_cluster",
        {"node1:9000", "node2:9000", "node3:9000"},
        "node1:9000");

    REQUIRE(coord != nullptr);

    auto started = coord->start();
    REQUIRE(started);
}
