# src/Databases/CMakeLists.txt
file(GLOB MNEMOSYNE_DATABASES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_databases ${MNEMOSYNE_DATABASES_SOURCES})

target_include_directories(mnemosyne_databases PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_databases PUBLIC
    mnemosyne_storages
    mnemosyne_datatypes
    mnemosyne_common
)

set_target_properties(mnemosyne_databases PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
