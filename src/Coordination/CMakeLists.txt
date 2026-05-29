# src/Coordination/CMakeLists.txt
add_library(mnemosyne_coordination
    coordination.cpp
)

target_include_directories(mnemosyne_coordination PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_coordination PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_coordination PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
