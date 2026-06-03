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
    REQUIRE(col->get(0).as_uint64().value() == static_cast<uint64_t>(1));
    REQUIRE(col->get(1).as_uint64().value() == static_cast<uint64_t>(2));
    REQUIRE(col->get(2).as_uint64().value() == static_cast<uint64_t>(3));
}

TEST_CASE("ColumnString insert and get", "[column]") {
    auto col = mnesso::columns::ColumnString::create();
    REQUIRE(col->size() == 0);

    col->insert(mnesso::core::Field(std::string{"Hello"}));
    col->insert(mnesso::core::Field(std::string{"World"}));

    REQUIRE(col->size() == 2);
    REQUIRE(col->get(0).as_string().value() == "Hello");
    REQUIRE(col->get(1).as_string().value() == "World");
}

TEST_CASE("Column swap rows", "[column]") {
    auto col = mnesso::columns::ColumnVector<int64_t>::create();
    col->insert(mnesso::core::Field{static_cast<int64_t>(1)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(2)});
    col->insert(mnesso::core::Field{static_cast<int64_t>(3)});

    col->swap_rows(0, 2);

    REQUIRE(col->get(0).as_int64().value() == static_cast<int64_t>(3));
    REQUIRE(col->get(1).as_int64().value() == static_cast<int64_t>(2));
    REQUIRE(col->get(2).as_int64().value() == static_cast<int64_t>(1));
}
