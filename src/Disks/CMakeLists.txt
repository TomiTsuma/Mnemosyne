# src/Disks/CMakeLists.txt
add_library(mnemosyne_disks
    disk.cpp
    disk_local.cpp
    disk_s3.cpp
)

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
