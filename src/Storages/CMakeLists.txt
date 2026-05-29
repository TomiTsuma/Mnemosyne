# src/Storages/CMakeLists.txt
add_library(mnemosyne_storages
    i_storage.cpp
    file_storage.cpp
    memory_storage.cpp
    dictionary_storage.cpp
    storage_factory.cpp
)

target_include_directories(mnemosyne_storages PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_storages PUBLIC
    mnemosyne_core
    mnemosyne_datatypes
    mnemosyne_disks
    mnemosyne_common
)

set_target_properties(mnemosyne_storages PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
