// tests/test_columns.h — Unit tests for columns
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "columns/column_vector.h"
#include "columns/column_string.h"
#include "core/field.h"
#include <catch2/catch_all.hpp>

// ── Column tests ──
TEST_CASE("ColumnVector UInt64 insert and get", "[column]") {
    auto col = mnesso::columns::ColumnVector<uint64_t>::create();
    REQUIRE(col->size() == 0);

    col->insert(mnesso::core::Field{static_cast<int64_t>(1)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(2)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(3)});

    REQUIRE(col->size() == 3);
    REQUIRE(col->get<uint64_t>(0) == 1);
    REQUIRE(col->get<uint64_t>(1) == 2);
    REQUIRE(col->get<uint64_t>(2) == 3);
}

TEST_CASE("ColumnString insert and get", "[column]") {
    auto col = mnesso::columns::ColumnString::create();
    REQUIRE(col->size() == 0);

    col->insert(mnesso::core::Field{"Hello"});
    col->insert(mnesso::core::Field{"World"});

    REQUIRE(col->size() == 2);
    REQUIRE(col->get(0) == "Hello");
    REQUIRE(col->get(1) == "World");
}

TEST_CASE("Column swap rows", "[column]") {
    auto col = mnesso::columns::ColumnVector<int64_t>::create();
    col->insert(mnesso::core::Field{static_cast<int64_t>(1)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(2)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(3)});

    col->swap_rows(0, 2);

    REQUIRE(col->get<int64_t>(0) == 3);
    REQUIRE(col->get<int64_t>(1) == 2);
    REQUIRE(col->get<int64_t>(2) == 1);
}
