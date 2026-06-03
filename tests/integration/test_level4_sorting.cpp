// tests/integration/test_level4_sorting.cpp — Level 4 Sorting integration tests
// Mnemosyne: A column-oriented analytical DBMS

#include <gtest/gtest.h>
#include "Interpreters/context.h"
#include "Databases/database_memory.h"
#include "Storages/memory_storage.h"
#include "Parsers/parser.h"
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Interpreters/query_executor.h"

namespace mnesso::tests {

class Level4SortingTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto db = std::make_shared<databases::DatabaseMemory>("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");

        // Create a test table
        std::vector<storages::IStorage::ColumnDef> columns = {
            {"id", "Int64"},
            {"name", "String"},
            {"score", "Float64"}
        };
        db->create_table("students", columns, "Memory");

        // Insert test data
        auto storage = db->get_table("students");
        core::Block block;
        auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
        auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
        auto score_col = std::make_shared<columns::ColumnVector<double>>();

        id_col->insert(3);
        name_col->insert("Charlie");
        score_col->insert(85.5);

        id_col->insert(1);
        name_col->insert("Alice");
        score_col->insert(95.0);

        id_col->insert(2);
        name_col->insert("Bob");
        score_col->insert(90.0);

        block.add_column("id", id_col);
        block.add_column("name", name_col);
        block.add_column("score", score_col);

        storage->write(block);
    }

    interpreters::Context context_;
};

TEST_F(Level4SortingTest, OrderByAsc) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM students ORDER BY id ASC";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level4SortingTest, OrderByDesc) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM students ORDER BY id DESC";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level4SortingTest, OrderByMultipleColumns) {
    parsers::Parser parser;
    std::string query = "SELECT id, name, score FROM students ORDER BY score DESC, id ASC";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level4SortingTest, OrderByDefaultAsc) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM students ORDER BY id";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

} // namespace mnesso::tests
