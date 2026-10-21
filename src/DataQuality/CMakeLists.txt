# src/DataQuality/CMakeLists.txt
file(GLOB MNEMOSYNE_DATA_QUALITY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_data_quality ${MNEMOSYNE_DATA_QUALITY_SOURCES})

target_include_directories(mnemosyne_data_quality PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_data_quality PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_data_quality PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
