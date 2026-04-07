#include <benchmark/benchmark.h>
#include <chrono>
#include <vector>
#include <numeric>

#include <cdr/base/generator.h>

using namespace cdr::internal;

Generator<uint64_t> Range(uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) {
        co_yield i;
    }
}

Generator<uint64_t> RecursiveRange(uint32_t depth, uint64_t n) {
    if (depth == 0) {
        for (uint64_t i = 0; i < n; ++i) {
            co_yield i;
        }
    } else {
        auto next = RecursiveRange(depth - 1, n);
        co_yield next;
    }
}

Generator<uint64_t> ErrorGen() {
    co_yield cdr::Failure(cdr::Error::__NumberOfErrors);
}

// co_yield cost
static void BM_Generator_Iteration_Baseline(benchmark::State& state) {
    const uint64_t n = state.range(0);
    for (auto _ : state) {
        uint64_t sum = 0;
        auto gen = Range(n);
        for (auto&& res : gen) {
            sum += res.Value();
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Generator_Iteration_Baseline)->Range(8, 8192);

// deep recursion
static void BM_Generator_Flattening_Depth(benchmark::State& state) {
    const uint32_t depth = state.range(0);
    constexpr uint64_t n = 1000;
    for (auto _ : state) {
        uint64_t sum = 0;
        auto gen = RecursiveRange(depth, n);
        for (auto&& res : gen) {
            sum += res.Value();
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Generator_Flattening_Depth)->Arg(1)->Arg(10)->Arg(50)->Arg(100);

// allocation cost
static void BM_Generator_Lifecycle(benchmark::State& state) {
    for (auto _ : state) {
        auto gen = Range(1);
        benchmark::DoNotOptimize(gen);
    }
}
BENCHMARK(BM_Generator_Lifecycle);

// compare with std::vector
static void BM_Vector_Baseline(benchmark::State& state) {
    const uint64_t n = state.range(0);
    for (auto _ : state) {
        std::vector<uint64_t> v(n);
        std::iota(v.begin(), v.end(), 0);

        uint64_t sum = 0;
        for (u64 x : v) {
            sum += x;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Vector_Baseline)->Range(8, 8192);

// error overhead
static void BM_Generator_ErrorPath(benchmark::State& state) {
    for (auto _ : state) {
        auto gen = ErrorGen();
        auto it = gen.begin();
        if (it != gen.end()) {
            auto res = *it;
            benchmark::DoNotOptimize(res);
        }
    }
}
BENCHMARK(BM_Generator_ErrorPath);

BENCHMARK_MAIN();