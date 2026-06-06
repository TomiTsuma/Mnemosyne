# src/Server/CMakeLists.txt
add_library(mnemosyne_server_lib
    server.cpp
    http_handler.cpp
    http_server.cpp
    tcp_server.cpp
)

target_include_directories(mnemosyne_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_server_lib PUBLIC
    mnemosyne_interpreters
    mnemosyne_analyzer
    mnemosyne_parsers
    mnemosyne_nodes
    mnemosyne_loggers
    mnemosyne_common
)

target_link_libraries(mnemosyne_server_lib PRIVATE
    httplib
)

set_target_properties(mnemosyne_server_lib PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
