# src/Loggers/CMakeLists.txt
add_library(mnemosyne_loggers
    logger.cpp
    target_console.cpp
    target_file.cpp
)

target_include_directories(mnemosyne_loggers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_loggers PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_loggers PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
