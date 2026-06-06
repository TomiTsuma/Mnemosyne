cmake_minimum_required(VERSION 3.28)

project(
mnemosyne
VERSION 0.1.0
DESCRIPTION "Mnemosyne: A Column-Oriented Analytical DBMS"
LANGUAGES C CXX
)

# ------------------------------------------------------------------------------

# Global Compiler Settings

# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
add_compile_options(
/EHsc
/Zc:__cplusplus
/std:c++latest
)
endif()

# ------------------------------------------------------------------------------

# Include Paths

# ------------------------------------------------------------------------------

include_directories(
${CMAKE_SOURCE_DIR}/src
)

# ------------------------------------------------------------------------------

# Build Options

# ------------------------------------------------------------------------------

option(ENABLE_TESTS "Enable test targets" ON)
option(ENABLE_BENCHMARK "Enable benchmark targets" OFF)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)

# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------

# Fetch cpp-httplib (single-header HTTP server library)

# ------------------------------------------------------------------------------

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.0
)
set(HTTPLIB_REQUIRE_SSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

# Compression libraries
FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.10.0
    SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(lz4)

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)  # also disables the v01-v07 legacy files
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)  # optional, removes threading.c
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(zstd)

message(STATUS "lz4 source dir: ${lz4_SOURCE_DIR}")
message(STATUS "zstd source dir: ${zstd_SOURCE_DIR}")

get_property(_lz4_targets DIRECTORY ${lz4_SOURCE_DIR}/build/cmake
             PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "lz4 targets: ${_lz4_targets}")

get_property(_zstd_targets DIRECTORY ${zstd_SOURCE_DIR}/build/cmake
             PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "zstd targets: ${_zstd_targets}")

# Core Components

# ------------------------------------------------------------------------------

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
add_subdirectory(src/StorageUnits)
add_subdirectory(src/Connectors)
add_subdirectory(src/Nodes)
add_subdirectory(src/ReplicaGroups)
add_subdirectory(src/ShardGroups)
add_subdirectory(src/Pipelines)
add_subdirectory(src/Streaming)
add_subdirectory(src/FeatureSets)
add_subdirectory(src/Models)
add_subdirectory(src/IO)
add_subdirectory(src/Loggers)
add_subdirectory(src/Server)
add_subdirectory(src/Coordination)
add_subdirectory(src/Backups)
add_subdirectory(src/Common)

# ------------------------------------------------------------------------------

# Programs

# ------------------------------------------------------------------------------

add_subdirectory(programs)

# ------------------------------------------------------------------------------

# Testing

# ------------------------------------------------------------------------------

if(ENABLE_TESTS)
include(CTest)
enable_testing()
add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------

# Benchmarks

# ------------------------------------------------------------------------------

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

if(ENABLE_BENCHMARK)
add_subdirectory(benchmark)
endif()
