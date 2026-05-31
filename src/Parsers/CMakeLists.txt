# src/Parsers/CMakeLists.txt
file(GLOB MNEMOSYNE_PARSERS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_parsers ${MNEMOSYNE_PARSERS_SOURCES})

target_include_directories(mnemosyne_parsers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_parsers PUBLIC
    mnemosyne_core
    mnemosyne_functions
)

set_target_properties(mnemosyne_parsers PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
