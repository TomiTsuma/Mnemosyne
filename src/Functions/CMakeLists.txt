# src/Functions/CMakeLists.txt
add_library(mnemosyne_functions
    function_factory.cpp
    arithmetic.cpp
    comparison.cpp
)

target_include_directories(mnemosyne_functions PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_functions PUBLIC
    mnemosyne_core
    mnemosyne_columns
    mnemosyne_datatypes
)

set_target_properties(mnemosyne_functions PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
