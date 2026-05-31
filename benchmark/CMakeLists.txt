# benchmark/CMakeLists.txt
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG main
    )
    # Allow the benchmark project to download its test dependency (googletest)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "Allow benchmark to download dependencies" FORCE)
    FetchContent_MakeAvailable(google_benchmark)
endif()
add_executable(mnemosyne_benchmark benchmark_main.cpp)
set_target_properties(mnemosyne_benchmark PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
target_link_libraries(mnemosyne_benchmark PRIVATE
    benchmark::benchmark
    mnemosyne_core
    mnemosyne_datatypes
    mnemosyne_columns
    mnemosyne_functions
)
