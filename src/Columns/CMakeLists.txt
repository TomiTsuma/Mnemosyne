# src/Columns/CMakeLists.txt
add_library(mnemosyne_columns
    column_vector.cpp
    column_string.cpp
    column_array.cpp
)

target_include_directories(mnemosyne_columns PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_columns PUBLIC
    mnemosyne_core
    mnemosyne_datatypes
)

set_target_properties(mnemosyne_columns PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
