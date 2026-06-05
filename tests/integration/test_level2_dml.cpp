// tests/integration/test_level2_dml.cpp — Level 2 DML operations integration tests
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

class Level2DMLTest : public ::testing::Test {
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
        db->create_table("users", columns, "Memory");
    }

    interpreters::Context context_;
};

TEST_F(Level2DMLTest, InsertSingleRow) {
    auto db = context_.get_database("test_db");
    auto storage = db->get_table("users");
    ASSERT_NE(storage, nullptr);

    core::Block block;
    auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
    auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
    auto value_col = std::make_shared<columns::ColumnVector<double>>();

    id_col->insert(1);
    name_col->insert("Alice");
    value_col->insert(100.5);

    block.add_column("id", id_col);
    block.add_column("name", name_col);
    block.add_column("value", value_col);

    storage->write(block);

    EXPECT_EQ(storage->row_count(), 1);
}

TEST_F(Level2DMLTest, InsertMultipleRows) {
    auto db = context_.get_database("test_db");
    auto storage = db->get_table("users");
    ASSERT_NE(storage, nullptr);

    core::Block block;
    auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
    auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
    auto value_col = std::make_shared<columns::ColumnVector<double>>();

    for (int i = 1; i <= 3; ++i) {
        id_col->insert(i);
        name_col->insert("User" + std::to_string(i));
        value_col->insert(100.0 * i);
    }

    block.add_column("id", id_col);
    block.add_column("name", name_col);
    block.add_column("value", value_col);

    storage->write(block);

    EXPECT_EQ(storage->row_count(), 3);
}

TEST_F(Level2DMLTest, SelectAllColumns) {
    auto db = context_.get_database("test_db");
    auto storage = db->get_table("users");
    ASSERT_NE(storage, nullptr);

    // Insert test data
    core::Block block;
    auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
    auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
    auto value_col = std::make_shared<columns::ColumnVector<double>>();

    id_col->insert(1);
    name_col->insert("Alice");
    value_col->insert(100.5);

    block.add_column("id", id_col);
    block.add_column("name", name_col);
    block.add_column("value", value_col);

    storage->write(block);

    // Read back
    auto read_block = storage->read(storage->columns(), 1000);
    EXPECT_EQ(read_block.row_count(), 1);
    EXPECT_EQ(read_block.column_count(), 3);
}

TEST_F(Level2DMLTest, SelectSpecificColumns) {
    auto db = context_.get_database("test_db");
    auto storage = db->get_table("users");
    ASSERT_NE(storage, nullptr);

    // Insert test data
    core::Block block;
    auto id_col = std::make_shared<columns::ColumnVector<int64_t>>();
    auto name_col = std::make_shared<columns::ColumnVector<std::string>>();
    auto value_col = std::make_shared<columns::ColumnVector<double>>();

    id_col->insert(1);
    name_col->insert("Alice");
    value_col->insert(100.5);

    block.add_column("id", id_col);
    block.add_column("name", name_col);
    block.add_column("value", value_col);

    storage->write(block);

    // Read specific columns
    std::vector<std::string> cols_to_read = {"id", "name"};
    auto read_block = storage->read(cols_to_read, 1000);
    EXPECT_EQ(read_block.row_count(), 1);
    EXPECT_EQ(read_block.column_count(), 2);
}

} // namespace mnemo::tests
