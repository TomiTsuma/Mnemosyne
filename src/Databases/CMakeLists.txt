# src/Databases/CMakeLists.txt
add_library(mnemosyne_databases
    i_database.cpp
    database_factory.cpp
    database_memory.cpp
)

target_include_directories(mnemosyne_databases PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_databases PUBLIC
    mnemosyne_storages
    mnemosyne_common
)

set_target_properties(mnemosyne_databases PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
