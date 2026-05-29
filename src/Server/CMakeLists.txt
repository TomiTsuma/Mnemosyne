# src/Server/CMakeLists.txt
add_library(mnemosyne_server
    server.cpp
    http_handler.cpp
    http_server.cpp
    tcp_server.cpp
)

target_include_directories(mnemosyne_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_server PUBLIC
    mnemosyne_interpreters
    mnemosyne_analyzer
    mnemosyne_parsers
    mnemosyne_loggers
    mnemosyne_common
)

target_link_libraries(mnemosyne_server PRIVATE
    httplib
)

set_target_properties(mnemosyne_server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
