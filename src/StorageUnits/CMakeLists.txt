# src/StorageUnits/CMakeLists.txt
file(GLOB MNEMOSYNE_STORAGE_UNITS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_storage_units ${MNEMOSYNE_STORAGE_UNITS_SOURCES})

target_include_directories(mnemosyne_storage_units PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_storage_units PUBLIC
    mnemosyne_disks
    mnemosyne_common
)

set_target_properties(mnemosyne_storage_units PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
