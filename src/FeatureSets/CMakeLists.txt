# src/FeatureSets/CMakeLists.txt
file(GLOB MNEMOSYNE_FEATURE_SETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_feature_sets ${MNEMOSYNE_FEATURE_SETS_SOURCES})

target_include_directories(mnemosyne_feature_sets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_feature_sets PUBLIC
    mnemosyne_core
    mnemosyne_columns
    mnemosyne_storages
    mnemosyne_common
)

set_target_properties(mnemosyne_feature_sets PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
