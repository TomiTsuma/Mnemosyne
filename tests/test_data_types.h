// tests/test_data_types.h — Unit tests for data types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "DataTypes/data_type.h"
#include "DataTypes/data_type_number.h"
#include "DataTypes/data_type_string.h"
#include "DataTypes/data_type_date.h"
#include "DataTypes/data_type_factory.h"
#include <catch2/catch_all.hpp>

// ── Data type tests ──
TEST_CASE("TypeId equality", "[data_type]") {
    REQUIRE(mnesso::datatypes::TypeId::UInt8 < mnesso::datatypes::TypeId::UInt16);
    REQUIRE(mnesso::datatypes::TypeId::Int64 < mnesso::datatypes::TypeId::Float64);
}

TEST_CASE("TypeRegistry singleton", "[data_type]") {
    auto& registry = mnesso::datatypes::TypeFactory::instance();
    auto& registry2 = mnesso::datatypes::TypeFactory::instance();
    REQUIRE(&registry == &registry2);
}

TEST_CASE("DataType registration", "[data_type]") {
    auto type = mnesso::datatypes::get_data_type("Int64");
    REQUIRE(type != nullptr);
    REQUIRE(type->name() == "Int64");
}
