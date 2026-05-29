# src/Analyzer/CMakeLists.txt
add_library(mnemosyne_analyzer
    analyzer.cpp
    analyzer_passes.cpp
)

target_include_directories(mnemosyne_analyzer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_analyzer PUBLIC
    mnemosyne_parsers
    mnemosyne_interpreters
)

set_target_properties(mnemosyne_analyzer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
