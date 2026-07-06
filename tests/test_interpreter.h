// tests/test_interpreter.h — Unit tests for query interpreter
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "interpreters/interpreter.h"
#include "interpreters/query_executor.h"
#include "interpreters/interpreter_select_query.h"
#include "planner/execution_plan.h"
#include "Databases/database_memory.h"
#include "Storages/memory_storage.h"
#include "Columns/column_vector.h"
#include "Columns/column_string.h"
#include "DataTypes/data_type_factory.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include <catch2/catch_all.hpp>

namespace mnemo::tests {

inline auto make_customers_db(mnemo::interpreters::Context& context) -> void {
    auto db = mnemo::databases::DatabaseMemory::create("test_db");
    context.register_database("test_db", db);
    context.set_current_database("test_db");

    std::unordered_map<std::string, mnemo::datatypes::DataTypePtr> columns = {
        {"customer_id", mnemo::datatypes::get_data_type("Int64")},
        {"name", mnemo::datatypes::get_data_type("String")},
        {"age", mnemo::datatypes::get_data_type("Int64")},
    };
    auto storage = db->create_table("customers", columns, "Memory");

    mnemo::core::Block block;
    auto id_col = std::make_shared<mnemo::columns::ColumnVector<int64_t>>();
    auto name_col = std::make_shared<mnemo::columns::ColumnString>();
    auto age_col = std::make_shared<mnemo::columns::ColumnVector<int64_t>>();
    const std::vector<std::tuple<int64_t, std::string, int64_t>> rows = {
        {1, "Alice", 28}, {2, "Bob", 22}, {3, "Carl", 27}, {4, "Dana", 21}, {5, "Eve", 35},
    };
    for (const auto& [id, name, age] : rows) {
        id_col->insert(mnemo::core::Field{id});
        name_col->insert(mnemo::core::Field{name});
        age_col->insert(mnemo::core::Field{age});
    }
    block.add_column("customer_id", id_col);
    block.add_column("name", name_col);
    block.add_column("age", age_col);
    storage->write(block);
}

inline auto parse(const std::string& sql) -> std::unique_ptr<mnemo::parsers::QueryAST> {
    mnemo::parsers::Lexer lexer{sql};
    mnemo::parsers::QueryParser parser{std::move(lexer)};
    return parser.parse();
}

} // namespace mnemo::tests

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

TEST_CASE("InterpreterSelectQuery keeps AS alias on aggregate output", "[interpreter][select]") {
    mnemo::interpreters::Context context;
    mnemo::tests::make_customers_db(context);

    auto ast = mnemo::tests::parse("SELECT COUNT(name) as total_customers FROM customers");
    REQUIRE(ast != nullptr);

    auto result = mnemo::interpreters::InterpreterSelectQuery::execute(context, *ast);
    REQUIRE(result.column_names().size() == 1);
    REQUIRE(result.column_names()[0] == "total_customers");
    REQUIRE(result.row_count() == 1);
}

TEST_CASE("InterpreterSelectQuery applies LIMIT", "[interpreter][select]") {
    mnemo::interpreters::Context context;
    mnemo::tests::make_customers_db(context);

    auto ast = mnemo::tests::parse("SELECT customer_id, age FROM customers LIMIT 3");
    REQUIRE(ast != nullptr);

    auto result = mnemo::interpreters::InterpreterSelectQuery::execute(context, *ast);
    REQUIRE(result.row_count() == 3);
}

// ages: Alice 28, Bob 22, Carl 27, Dana 21, Eve 35 → sorted: 21, 22, 27, 28, 35
TEST_CASE("InterpreterSelectQuery computes QUANTILE aggregate", "[interpreter][select]") {
    mnemo::interpreters::Context context;
    mnemo::tests::make_customers_db(context);

    auto ast = mnemo::tests::parse("SELECT QUANTILE(age, 0.5) as median_age FROM customers");
    REQUIRE(ast != nullptr);

    auto result = mnemo::interpreters::InterpreterSelectQuery::execute(context, *ast);
    REQUIRE(result.row_count() == 1);
    REQUIRE(result.column_names()[0] == "median_age");
    auto median = result.get_row_value(0, 0).as_float64();
    REQUIRE(median.has_value());
    REQUIRE(*median == Catch::Approx(27.0));
}

TEST_CASE("InterpreterSelectQuery computes PERCENTILE aggregate on a 0-100 scale", "[interpreter][select]") {
    mnemo::interpreters::Context context;
    mnemo::tests::make_customers_db(context);

    auto ast = mnemo::tests::parse("SELECT PERCENTILE(age, 50) as median_age FROM customers");
    REQUIRE(ast != nullptr);

    auto result = mnemo::interpreters::InterpreterSelectQuery::execute(context, *ast);
    REQUIRE(result.row_count() == 1);
    auto median = result.get_row_value(0, 0).as_float64();
    REQUIRE(median.has_value());
    REQUIRE(*median == Catch::Approx(27.0));
}

TEST_CASE("InterpreterSelectQuery computes NTILE window buckets", "[interpreter][select]") {
    mnemo::interpreters::Context context;
    mnemo::tests::make_customers_db(context);

    auto ast = mnemo::tests::parse("SELECT name, age, NTILE(2) OVER (ORDER BY age) FROM customers");
    REQUIRE(ast != nullptr);

    auto result = mnemo::interpreters::InterpreterSelectQuery::execute(context, *ast);
    REQUIRE(result.row_count() == 5);

    auto name_col = result.get_column("name");
    auto tile_col = result.get_column("NTILE(2)_over");
    REQUIRE(name_col != nullptr);
    REQUIRE(tile_col != nullptr);

    std::unordered_map<std::string, int64_t> tile_by_name;
    for (size_t r = 0; r < result.row_count(); ++r) {
        auto name = name_col->get(r).as_string();
        auto tile = tile_col->get(r).as_int64();
        REQUIRE(name.has_value());
        REQUIRE(tile.has_value());
        tile_by_name[*name] = *tile;
    }

    // Lower half by age (Dana 21, Bob 22, Carl 27) → tile 1; upper half (Alice 28, Eve 35) → tile 2.
    REQUIRE(tile_by_name["Dana"] == 1);
    REQUIRE(tile_by_name["Bob"] == 1);
    REQUIRE(tile_by_name["Carl"] == 1);
    REQUIRE(tile_by_name["Alice"] == 2);
    REQUIRE(tile_by_name["Eve"] == 2);
}
