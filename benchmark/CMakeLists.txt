# benchmark/CMakeLists.txt
find_package(benchmark REQUIRED)
add_executable(mnemosyne_benchmark benchmark_main.cpp)
target_link_libraries(mnemosyne_benchmark PRIVATE
    benchmark::benchmark
    mnemosyne_core
    mnemosyne_datatypes
    mnemosyne_columns
    mnemosyne_functions
)
