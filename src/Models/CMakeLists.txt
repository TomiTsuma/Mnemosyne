# src/Models/CMakeLists.txt
file(GLOB MNEMOSYNE_MODELS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_models ${MNEMOSYNE_MODELS_SOURCES})

target_include_directories(mnemosyne_models PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_models PUBLIC
    mnemosyne_core
    mnemosyne_columns
    mnemosyne_feature_sets
    mnemosyne_common
)

set_target_properties(mnemosyne_models PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
