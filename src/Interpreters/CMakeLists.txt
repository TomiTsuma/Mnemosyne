# src/Interpreters/CMakeLists.txt
file(GLOB MNEMOSYNE_INTERPRETERS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_interpreters ${MNEMOSYNE_INTERPRETERS_SOURCES})

target_include_directories(mnemosyne_interpreters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_interpreters PUBLIC
    mnemosyne_planner
    mnemosyne_processors
    mnemosyne_storages
    mnemosyne_databases
    mnemosyne_storage_units
    mnemosyne_connectors
    mnemosyne_nodes
    mnemosyne_replica_groups
    mnemosyne_shard_groups
    mnemosyne_pipelines
    mnemosyne_streaming
    mnemosyne_feature_sets
    mnemosyne_models
    mnemosyne_common
    mnemosyne_loggers
)

set_target_properties(mnemosyne_interpreters PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
