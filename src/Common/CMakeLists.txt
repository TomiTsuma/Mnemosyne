# src/Common/CMakeLists.txt
add_library(mnemosyne_common
    settings.cpp
    logging.cpp
    exceptions.cpp
    thread_pool.cpp
)

target_include_directories(mnemosyne_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_common PUBLIC
    std::format
)

set_target_properties(mnemosyne_common PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
