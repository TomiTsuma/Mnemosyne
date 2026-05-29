# src/AggregateFunctions/CMakeLists.txt
add_library(mnemosyne_aggregate_functions
    i_aggregate_function.cpp
    sum.cpp
    count.cpp
    avg.cpp
    min_max.cpp
    aggregate_function_factory.cpp
)

target_include_directories(mnemosyne_aggregate_functions PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_aggregate_functions PUBLIC
    mnemosyne_core
    mnemosyne_functions
)

set_target_properties(mnemosyne_aggregate_functions PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
