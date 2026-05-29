# src/Backups/CMakeLists.txt
add_library(mnemosyne_backups
    backup.cpp
)

target_include_directories(mnemosyne_backups PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_backups PUBLIC
    mnemosyne_storages
    mnemosyne_common
)

set_target_properties(mnemosyne_backups PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
