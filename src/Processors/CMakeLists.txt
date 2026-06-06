# src/Processors/CMakeLists.txt
file(GLOB MNEMOSYNE_PROCESSORS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_processors ${MNEMOSYNE_PROCESSORS_SOURCES})

target_include_directories(mnemosyne_processors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_processors PUBLIC
    mnemosyne_core
    mnemosyne_databases
    mnemosyne_datatypes
    mnemosyne_storages
)

set_target_properties(mnemosyne_processors PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
