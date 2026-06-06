# src/ShardGroups/CMakeLists.txt
file(GLOB MNEMOSYNE_SHARD_GROUPS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_shard_groups ${MNEMOSYNE_SHARD_GROUPS_SOURCES})

target_include_directories(mnemosyne_shard_groups PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mnemosyne_shard_groups PUBLIC
    mnemosyne_nodes
    mnemosyne_common
)

set_target_properties(mnemosyne_shard_groups PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
