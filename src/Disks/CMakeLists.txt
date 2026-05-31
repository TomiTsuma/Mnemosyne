# src/Disks/CMakeLists.txt
file(GLOB MNEMOSYNE_DISKS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_disks ${MNEMOSYNE_DISKS_SOURCES})

target_include_directories(mnemosyne_disks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_disks PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_disks PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
