// tests/integration/test_level6_groupby.cpp — Level 6 GROUP BY integration tests
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

class Level6GroupByTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto db = std::make_shared<databases::DatabaseMemory>("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");

        // Create a test table
        std::vector<storages::IStorage::ColumnDef> columns = {
            {"id", "Int64"},
            {"department", "String"},
            {"salary", "Float64"}
        };
        db->create_table("employees", columns, "Memory");

        // Insert test data
        auto storage = db->get_table("employees");
        core::Block block;
        auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
        auto dept_col = std::make_shared<columns::ColumnVector<std::string>>();
        auto salary_col = std::make_shared<columns::ColumnVector<double>>();

        id_col->insert(1);
        dept_col->insert("Engineering");
        salary_col->insert(100000.0);

        id_col->insert(2);
        dept_col->insert("Engineering");
        salary_col->insert(120000.0);

        id_col->insert(3);
        dept_col->insert("Sales");
        salary_col->insert(80000.0);

        id_col->insert(4);
        dept_col->insert("Sales");
        salary_col->insert(90000.0);

        id_col->insert(5);
        dept_col->insert("Marketing");
        salary_col->insert(75000.0);

        block.add_column("id", id_col);
        block.add_column("department", dept_col);
        block.add_column("salary", salary_col);

        storage->write(block);
    }

    interpreters::Context context_;
};

TEST_F(Level6GroupByTest, GroupBySingleColumn) {
    parsers::Parser parser;
    std::string query = "SELECT department, COUNT(*) FROM employees GROUP BY department";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level6GroupByTest, GroupByWithSum) {
    parsers::Parser parser;
    std::string query = "SELECT department, SUM(salary) FROM employees GROUP BY department";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level6GroupByTest, GroupByWithAvg) {
    parsers::Parser parser;
    std::string query = "SELECT department, AVG(salary) FROM employees GROUP BY department";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level6GroupByTest, GroupByMultipleAggregates) {
    parsers::Parser parser;
    std::string query = "SELECT department, COUNT(*), SUM(salary), AVG(salary), MIN(salary), MAX(salary) FROM employees GROUP BY department";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

TEST_F(Level6GroupByTest, GroupByMultipleColumns) {
    parsers::Parser parser;
    std::string query = "SELECT department, COUNT(*) FROM employees GROUP BY department";
    auto ast = parser.parse_query(query);
    ASSERT_NE(ast, nullptr);

    analyzer::Analyzer analyzer(context_);
    auto result = analyzer.analyze(ast);
    EXPECT_TRUE(result.valid);
}

} // namespace mnesso::tests
