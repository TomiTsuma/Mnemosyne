# src/Interpreters/CMakeLists.txt
add_library(mnemosyne_interpreters
    context.cpp
    query_executor.cpp
    blockInterpreter.cpp
)

target_include_directories(mnemosyne_interpreters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_interpreters PUBLIC
    mnemosyne_planner
    mnemosyne_processors
    mnemosyne_storages
    mnemosyne_databases
    mnemosyne_common
    mnemosyne_loggers
)

set_target_properties(mnemosyne_interpreters PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
