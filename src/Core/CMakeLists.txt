# src/Core/CMakeLists.txt
add_library(mnemosyne_core
    block.cpp
    field.cpp
)

target_include_directories(mnemosyne_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_core PUBLIC
    mnemosyne_common
)

target_link_libraries(mnemosyne_core PRIVATE
    std::format
)

set_target_properties(mnemosyne_core PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
