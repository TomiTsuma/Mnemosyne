# src/ReplicaGroups/CMakeLists.txt
file(GLOB MNEMOSYNE_REPLICA_GROUPS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_replica_groups ${MNEMOSYNE_REPLICA_GROUPS_SOURCES})

target_include_directories(mnemosyne_replica_groups PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mnemosyne_replica_groups PUBLIC
    mnemosyne_nodes
    mnemosyne_common
)

set_target_properties(mnemosyne_replica_groups PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
