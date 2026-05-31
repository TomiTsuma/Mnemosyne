# programs/client/CMakeLists.txt
add_executable(mnemosyne_client
    main.cpp
)

target_include_directories(mnemosyne_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_client PRIVATE
    mnemosyne_common
    mnemosyne_loggers
    mnemosyne_parsers
    mnemosyne_interpreters
    mnemosyne_server_lib
)

set_target_properties(mnemosyne_client PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
