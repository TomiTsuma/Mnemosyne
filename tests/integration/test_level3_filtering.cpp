// tests/integration/test_level3_filtering.cpp — Level 3 Filtering integration tests
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

class Level3FilteringTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto db = std::make_shared<databases::DatabaseMemory>("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");

        // Create a test table
        std::vector<storages::IStorage::ColumnDef> columns = {
            {"id", "Int64"},
            {"name", "String"},
            {"age", "Int64"}
        };
        db->create_table("users", columns, "Memory");

        // Insert test data
        auto storage = db->get_table("users");
        core::Block block;
        auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
        auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
        auto age_col = std::make_shared<columns::ColumnVector<int64_t>>();

        id_col->insert(1);
        name_col->insert("Alice");
        age_col->insert(25);

        id_col->insert(2);
        name_col->insert("Bob");
        age_col->insert(30);

        id_col->insert(3);
        name_col->insert("Charlie");
        age_col->insert(25);

        block.add_column("id", id_col);
        block.add_column("name", name_col);
        block.add_column("age", age_col);

        storage->write(block);
    }

    interpreters::Context context_;
};

TEST_F(Level3FilteringTest, WhereSinglePredicate) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM users WHERE age = 25";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level3FilteringTest, WhereAndCondition) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM users WHERE age = 25 AND id > 1";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level3FilteringTest, WhereOrCondition) {
    parsers::Parser parser;
    std::string query = "SELECT id, name FROM users WHERE age = 25 OR age = 30";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level3FilteringTest, WhereComparisonOperators) {
    parsers::Parser parser;
    
    std::vector<std::string> queries = {
        "SELECT * FROM users WHERE age > 25",
        "SELECT * FROM users WHERE age >= 25",
        "SELECT * FROM users WHERE age < 30",
        "SELECT * FROM users WHERE age <= 30",
        "SELECT * FROM users WHERE age != 25"
    };

    for (const auto& query : queries) {
        auto ast = parser.parse_query(query);
        ASSERT_NE(ast, nullptr) << "Failed to parse: " << query;

        analyzer::Analyzer analyzer(context_);
        auto result = analyzer.analyze(ast);
        EXPECT_TRUE(result.valid) << "Analysis failed for: " << query;
    }
}

} // namespace mnemo::tests
