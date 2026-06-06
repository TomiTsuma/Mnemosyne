# src/Connectors/CMakeLists.txt
file(GLOB MNEMOSYNE_CONNECTORS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_connectors ${MNEMOSYNE_CONNECTORS_SOURCES})

target_include_directories(mnemosyne_connectors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_connectors PUBLIC
    mnemosyne_columns
    mnemosyne_common
)

target_link_libraries(mnemosyne_connectors PRIVATE
    httplib
)

set_target_properties(mnemosyne_connectors PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
