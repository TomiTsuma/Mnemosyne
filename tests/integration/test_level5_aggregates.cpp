// tests/integration/test_level5_aggregates.cpp — Level 5 Aggregates integration tests
// Mnemosyne: A column-oriented analytical DBMS

#include <gtest/gtest.h>
#include "Interpreters/context.h"
#include "Databases/database_memory.h"
#include "Storages/memory_storage.h"
#include "Parsers/parser.h"
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Interpreters/query_executor.h"

namespace mnemo::tests {

class Level5AggregatesTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto db = std::make_shared<databases::DatabaseMemory>("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");

        // Create a test table
        std::vector<storages::IStorage::ColumnDef> columns = {
            {"id", "Int64"},
            {"name", "String"},
            {"value", "Float64"}
        };
        db->create_table("products", columns, "Memory");

        // Insert test data
        auto storage = db->get_table("products");
        core::Block block;
        auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
        auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
        auto value_col = std::make_shared<columns::ColumnVector<double>>();

        id_col->insert(1);
        name_col->insert("Product A");
        value_col->insert(100.0);

        id_col->insert(2);
        name_col->insert("Product B");
        value_col->insert(200.0);

        id_col->insert(3);
        name_col->insert("Product C");
        value_col->insert(300.0);

        block.add_column("id", id_col);
        block.add_column("name", name_col);
        block.add_column("value", value_col);

        storage->write(block);
    }

    interpreters::Context context_;
};

TEST_F(Level5AggregatesTest, Count) {
    parsers::Parser parser;
    std::string query = "SELECT COUNT(*) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level5AggregatesTest, Sum) {
    parsers::Parser parser;
    std::string query = "SELECT SUM(value) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level5AggregatesTest, Avg) {
    parsers::Parser parser;
    std::string query = "SELECT AVG(value) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level5AggregatesTest, Min) {
    parsers::Parser parser;
    std::string query = "SELECT MIN(value) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level5AggregatesTest, Max) {
    parsers::Parser parser;
    std::string query = "SELECT MAX(value) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level5AggregatesTest, MultipleAggregates) {
    parsers::Parser parser;
    std::string query = "SELECT COUNT(*), SUM(value), AVG(value), MIN(value), MAX(value) FROM products";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

} // namespace mnemo::tests
