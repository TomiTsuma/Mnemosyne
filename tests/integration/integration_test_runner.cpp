// tests/integration/integration_test_runner.cpp — Integration test runner for Levels 1-7
// Mnemosyne: A column-oriented analytical DBMS

#include <gtest/gtest.h>

// Include all level test files
#include "test_level1_ddl.cpp"
#include "test_level2_dml.cpp"
#include "test_level3_filtering.cpp"
#include "test_level4_sorting.cpp"
#include "test_level5_aggregates.cpp"
#include "test_level6_groupby.cpp"
#include "test_level7_having.cpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
