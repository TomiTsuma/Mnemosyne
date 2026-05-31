# src/Loggers/CMakeLists.txt
file(GLOB MNEMOSYNE_LOGGERS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_loggers ${MNEMOSYNE_LOGGERS_SOURCES})

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
