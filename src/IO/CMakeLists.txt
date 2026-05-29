# src/IO/CMakeLists.txt
add_library(mnemosyne_io
    codec.cpp
    lz4.cpp
    zstd.cpp
    native.cpp
)

target_include_directories(mnemosyne_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_io PUBLIC
    mnemosyne_common
)

target_link_libraries(mnemosyne_io PRIVATE
    lz4
    zstd
)

set_target_properties(mnemosyne_io PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
