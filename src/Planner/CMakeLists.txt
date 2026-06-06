# src/Planner/CMakeLists.txt
add_library(mnemosyne_planner
    planner.cpp
    execution_plan.cpp
)

target_include_directories(mnemosyne_planner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_planner PUBLIC
    mnemosyne_analyzer
    mnemosyne_storages
    mnemosyne_nodes
)

set_target_properties(mnemosyne_planner PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
