# tests/CMakeLists.txt
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.3.2
    )
    FetchContent_MakeAvailable(Catch2)
endif()

include(Catch)

add_executable(mnemosyne_tests test_all.cpp)
set_target_properties(mnemosyne_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
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
    mnemosyne_server_lib
    mnemosyne_backups
    mnemosyne_coordination
    mnemosyne_common
)

catch_discover_tests(mnemosyne_tests)
