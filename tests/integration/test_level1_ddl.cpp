// tests/integration/test_level1_ddl.cpp — Level 1 DDL operations integration tests
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

class Level1DDLTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a default database
        auto db = std::make_shared<databases::DatabaseMemory>("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");
    }

    interpreters::Context context_;
};

TEST_F(Level1DDLTest, CreateDatabase) {
    auto& db_manager = databases::DatabaseManager::instance();
    auto db = db_manager.create_database("new_db");
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->name(), "new_db");
}

TEST_F(Level1DDLTest, UseDatabase) {
    context_.set_current_database("test_db");
    EXPECT_EQ(context_.current_database(), "test_db");
}

TEST_F(Level1DDLTest, ShowDatabases) {
    auto db_names = context_.databases();
    EXPECT_FALSE(db_names.empty());
    EXPECT_TRUE(std::find(db_names.begin(), db_names.end(), "test_db") != db_names.end());
}

TEST_F(Level1DDLTest, CreateTable) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::vector<storages::IStorage::ColumnDef> columns = {
        {"id", "Int64"},
        {"name", "String"},
        {"value", "Float64"}
    };

    db->create_table("test_table", columns, "Memory");
    EXPECT_TRUE(db->has_table("test_table"));
}

TEST_F(Level1DDLTest, ShowTables) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::vector<storages::IStorage::ColumnDef> columns = {
        {"id", "Int64"},
        {"name", "String"}
    };

    db->create_table("table1", columns, "Memory");
    db->create_table("table2", columns, "Memory");

    auto tables = db->list_tables();
    EXPECT_GE(tables.size(), 2);
}

TEST_F(Level1DDLTest, DescribeTable) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::vector<storages::IStorage::ColumnDef> columns = {
        {"id", "Int64"},
        {"name", "String"}
    };

    db->create_table("test_table", columns, "Memory");
    
    auto storage = db->get_table("test_table");
    ASSERT_NE(storage, nullptr);

    auto cols = storage->columns();
    EXPECT_EQ(cols.size(), 2);
    EXPECT_TRUE(std::find(cols.begin(), cols.end(), "id") != cols.end());
    EXPECT_TRUE(std::find(cols.begin(), cols.end(), "name") != cols.end());
}

TEST_F(Level1DDLTest, DropTable) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::vector<storages::IStorage::ColumnDef> columns = {
        {"id", "Int64"}
    };

    db->create_table("temp_table", columns, "Memory");
    EXPECT_TRUE(db->has_table("temp_table"));

    db->drop_table("temp_table");
    EXPECT_FALSE(db->has_table("temp_table"));
}

} // namespace mnesso::tests
