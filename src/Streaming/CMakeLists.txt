# src/Streaming/CMakeLists.txt
file(GLOB MNEMOSYNE_STREAMING_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_streaming ${MNEMOSYNE_STREAMING_SOURCES})

target_include_directories(mnemosyne_streaming PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_streaming PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_streaming PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
