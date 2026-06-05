// tests/integration/test_level1_ddl.cpp — Level 1 DDL operations integration tests
// Mnemosyne: A column-oriented analytical DBMS

#include <gtest/gtest.h>
#include "Interpreters/context.h"
#include "Databases/database_memory.h"
#include "Databases/database_manager.h"
#include "DataTypes/data_type_factory.h"
#include "Storages/memory_storage.h"
#include "Parsers/parser.h"
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Interpreters/query_executor.h"

namespace mnemo::tests {

class Level1DDLTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a default database using the static factory method
        auto db = databases::DatabaseMemory::create("test_db");
        context_.register_database("test_db", db);
        context_.set_current_database("test_db");
    }

    interpreters::Context context_;
};

TEST_F(Level1DDLTest, CreateDatabase) {
    auto &db_manager = databases::DatabaseManager::instance();
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

    std::unordered_map<std::string, datatypes::DataTypePtr> columns = {
        {"id",    datatypes::get_data_type("Int64")},
        {"name",  datatypes::get_data_type("String")},
        {"value", datatypes::get_data_type("Float64")}
    };

    db->create_table("test_table", columns, "Memory");
    EXPECT_TRUE(db->table_exists("test_table"));
}

TEST_F(Level1DDLTest, ShowTables) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::unordered_map<std::string, datatypes::DataTypePtr> columns = {
        {"id",   datatypes::get_data_type("Int64")},
        {"name", datatypes::get_data_type("String")}
    };

    db->create_table("table1", columns, "Memory");
    db->create_table("table2", columns, "Memory");

    auto tables = db->tables();
    EXPECT_GE(tables.size(), 2u);
}

TEST_F(Level1DDLTest, DescribeTable) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::unordered_map<std::string, datatypes::DataTypePtr> columns = {
        {"id",   datatypes::get_data_type("Int64")},
        {"name", datatypes::get_data_type("String")}
    };

    db->create_table("test_table", columns, "Memory");
    auto storage = db->table("test_table");
    ASSERT_NE(storage, nullptr);

    auto col_names = storage->columns();
    EXPECT_EQ(col_names.size(), 2u);
    EXPECT_TRUE(std::find(col_names.begin(), col_names.end(), "id") != col_names.end());
    EXPECT_TRUE(std::find(col_names.begin(), col_names.end(), "name") != col_names.end());
}

TEST_F(Level1DDLTest, DropTable) {
    auto db = context_.get_database("test_db");
    ASSERT_NE(db, nullptr);

    std::unordered_map<std::string, datatypes::DataTypePtr> columns = {
        {"id", datatypes::get_data_type("Int64")}
    };

    db->create_table("temp_table", columns, "Memory");
    EXPECT_TRUE(db->table_exists("temp_table"));

    db->drop_table("temp_table");
    EXPECT_FALSE(db->table_exists("temp_table"));
}

} // namespace mnemo::tests
