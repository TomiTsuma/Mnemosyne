# tests/CMakeLists.txt
find_package(Catch2 3 REQUIRED)
include(Catch)

add_library(mnemosyne_tests test_all.cpp)
target_include_directories(mnemosyne_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_tests PRIVATE
    Catch2::Catch2WithMain
    mnemosyne_core
    mnemosyne_datatypes
    mnemosyne_columns
    mnemosyne_functions
    mnemosyne_aggregate_functions
    mnemosyne_parsers
    mnemosyne_analyzer
    mnemosyne_planner
    mnemosyne_interpreters
    mnemosyne_processors
    mnemosyne_storages
    mnemosyne_databases
    mnemosyne_disks
    mnemosyne_io
    mnemosyne_loggers
    mnemosyne_server
    mnemosyne_backups
    mnemosyne_coordination
    mnemosyne_common
)

catch_discover_tests(mnemosyne_tests)
