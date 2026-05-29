# src/Processors/CMakeLists.txt
add_library(mnemosyne_processors
    processor.cpp
    processors_source.cpp
)

target_include_directories(mnemosyne_processors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_processors PUBLIC
    mnemosyne_core
)

set_target_properties(mnemosyne_processors PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
