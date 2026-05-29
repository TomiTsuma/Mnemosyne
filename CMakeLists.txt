# Mnemosyne: A column-oriented analytical DBMS
# CMakeLists.txt — Top-level build file

cmake_minimum_required(VERSION 3.28)
project(mnemosyne
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Options ──
option(ENABLE_TESTS    "Enable test targets"    ON)
option(ENABLE_BENCHMARK "Enable benchmark targets" ON)
option(ENABLE_SIMD     "Enable SIMD optimizations" ON)

# ── Subdirectories ──
add_subdirectory(src/Core)
add_subdirectory(src/DataTypes)
add_subdirectory(src/Columns)
add_subdirectory(src/Functions)
add_subdirectory(src/AggregateFunctions)
add_subdirectory(src/Parsers)
add_subdirectory(src/Analyzer)
add_subdirectory(src/Planner)
add_subdirectory(src/Interpreters)
add_subdirectory(src/Processors)
add_subdirectory(src/Storages)
add_subdirectory(src/Databases)
add_subdirectory(src/Disks)
add_subdirectory(src/IO)
add_subdirectory(src/Loggers)
add_subdirectory(src/Server)
add_subdirectory(src/Coordination)
add_subdirectory(src/Backups)
add_subdirectory(src/Common)

# ── Programs ──
add_subdirectory(programs)

# ── Tests ──
if(ENABLE_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Benchmarks ──
if(ENABLE_BENCHMARK)
    add_subdirectory(benchmark)
endif()
