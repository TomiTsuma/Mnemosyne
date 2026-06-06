# src/Pipelines/CMakeLists.txt
file(GLOB MNEMOSYNE_PIPELINES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_pipelines ${MNEMOSYNE_PIPELINES_SOURCES})

target_include_directories(mnemosyne_pipelines PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_pipelines PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_pipelines PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
