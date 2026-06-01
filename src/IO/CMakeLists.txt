# src/IO/CMakeLists.txt
file(GLOB MNEMOSYNE_IO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_io ${MNEMOSYNE_IO_SOURCES})

target_include_directories(mnemosyne_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_io PUBLIC
    mnemosyne_common
)

target_link_libraries(mnemosyne_io PRIVATE
    lz4_static
    libzstd_static 
)

set_target_properties(mnemosyne_io PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
