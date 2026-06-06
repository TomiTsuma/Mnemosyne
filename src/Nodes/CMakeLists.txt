# src/Nodes/CMakeLists.txt
file(GLOB MNEMOSYNE_NODES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_nodes ${MNEMOSYNE_NODES_SOURCES})

target_include_directories(mnemosyne_nodes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mnemosyne_nodes PUBLIC
    mnemosyne_common
)

if(WIN32)
    target_link_libraries(mnemosyne_nodes PRIVATE ws2_32)
endif()

set_target_properties(mnemosyne_nodes PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
