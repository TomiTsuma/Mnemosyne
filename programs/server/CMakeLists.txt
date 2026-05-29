# programs/server/CMakeLists.txt
add_executable(mnemosyne_server
    main.cpp
)

target_include_directories(mnemosyne_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_server PRIVATE
    mnemosyne_common
    mnemosyne_loggers
    mnemosyne_databases
    mnemosyne_storages
    mnemosyne_interpreters
    mnemosyne_server
    mnemosyne_coordination
    mnemosyne_backups
)

set_target_properties(mnemosyne_server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
