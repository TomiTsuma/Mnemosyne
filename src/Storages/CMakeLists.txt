# src/Storages/CMakeLists.txt
file(GLOB MNEMOSYNE_STORAGES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_storages ${MNEMOSYNE_STORAGES_SOURCES})

target_include_directories(mnemosyne_storages PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_storages PUBLIC
    mnemosyne_core
    mnemosyne_columns
    mnemosyne_datatypes
    mnemosyne_disks
    mnemosyne_common
)

set_target_properties(mnemosyne_storages PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
