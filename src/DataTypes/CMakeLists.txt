# src/DataTypes/CMakeLists.txt
add_library(mnemosyne_datatypes
    data_type.cpp
    data_type_number.cpp
    data_type_string.cpp
    data_type_date.cpp
    data_type_factory.cpp
)

target_include_directories(mnemosyne_datatypes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_datatypes PUBLIC
    mnemosyne_core
)

set_target_properties(mnemosyne_datatypes PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
