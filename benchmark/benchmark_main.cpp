// benchmark/benchmark_main.cpp — Benchmark driver
// Mnemosyne: A column-oriented analytical DBMS

#include <benchmark/benchmark.h>
#include "core/block.h"
#include "core/series.h"
#include "DataTypes/data_type_number.h"
#include "columns/column_vector.h"
#include "columns/column_string.h"

// ── Benchmark: ColumnVector insert ──
static void BM_ColumnVectorInsert(benchmark::State& state) {
    auto col = mnemo::columns::ColumnVector<uint64_t>::create();
    for (auto _ : state) {
        col->insert(mnemo::core::Field{static_cast<int64_t>(state.iterations())});
    }
}
BENCHMARK(BM_ColumnVectorInsert);

// ── Benchmark: ColumnVector get ──
static void BM_ColumnVectorGet(benchmark::State& state) {
    auto col = mnemo::columns::ColumnVector<uint64_t>::create();
    for (size_t i = 0; i < state.range(0); ++i) {
        col->insert(mnemo::core::Field{static_cast<int64_t>(i)});
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(col->get(0));
    }
}
BENCHMARK(BM_ColumnVectorGet)->Arg(1000);

// ── Benchmark: ColumnString insert ──
static void BM_ColumnStringInsert(benchmark::State& state) {
    auto col = mnemo::columns::ColumnString::create();
    for (auto _ : state) {
        col->insert(mnemo::core::Field{
            std::string("Hello World, this is a benchmark string")});
    }
}
BENCHMARK(BM_ColumnStringInsert);

// ── Benchmark: Series arithmetic ──
static void BM_SeriesAdd(benchmark::State& state) {
    mnemo::core::Series<double> a(1024);
    mnemo::core::Series<double> b(1024);
    mnemo::core::Series<double> c(1024);

    for (size_t i = 0; i < 1024; ++i) {
        a[i] = static_cast<double>(i);
        b[i] = static_cast<double>(i * 2);
    }

    for (auto _ : state) {
        for (size_t i = 0; i < 1024; ++i) {
            c[i] = a[i] + b[i];
        }
    }
    benchmark::DoNotOptimize(c);
}
BENCHMARK(BM_SeriesAdd);

// ── Entry point ──
BENCHMARK_MAIN();
