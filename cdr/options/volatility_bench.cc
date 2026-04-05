#include <benchmark/benchmark.h>
#include <cdr/calendar/date.h>
#include <cdr/options/volatility.h>

#include <chrono>
#include <map>
#include <random>
#include <vector>

namespace {

inline double ToDays(const cdr::DateType& d) {
    auto sd = std::chrono::sys_days{d};
    return static_cast<double>(sd.time_since_epoch().count());
}

class NaiveMapSurface {
public:
    std::map<cdr::DateType, std::map<double, double>> data;

    double Volatility(const cdr::DateType& date, double strike) const {
        auto it2 = data.lower_bound(date);
        if (it2 == data.end()) return InterpolateStrike(std::prev(it2)->second, strike);
        if (it2 == data.begin()) return InterpolateStrike(it2->second, strike);

        auto it1 = std::prev(it2);

        double v1 = InterpolateStrike(it1->second, strike);
        double v2 = InterpolateStrike(it2->second, strike);

        double t1 = ToDays(it1->first);
        double t2 = ToDays(it2->first);
        double t = ToDays(date);

        return v1 + (v2 - v1) * (t - t1) / (t2 - t1);
    }

private:
    double InterpolateStrike(const std::map<double, double>& smile, double strike) const {
        auto it2 = smile.lower_bound(strike);
        if (it2 == smile.end()) return std::prev(it2)->second;
        if (it2 == smile.begin()) return it2->second;

        auto it1 = std::prev(it2);
        double s1 = it1->first;
        double s2 = it2->first;
        return it1->second + (it2->second - it1->second) * (strike - s1) / (s2 - s1);
    }
};

cdr::VolatilitySurface CreateFlatSurface(int num_dates, int num_strikes) {
    cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{today};
    for (int d = 1; d <= num_dates; ++d) {
        cdr::DateType expiry = cdr::AddDays(today, d * 30);
        for (int s = 0; s < num_strikes; ++s) {
            provider.AddPillar(expiry, 80.0 + s * 2.0, 0.20 + (s * 0.001)).OrCrashProgram();
        }
    }
    provider.UpdateSnapshot().OrCrashProgram();
    return provider.ProvideSnapshot().Value();
}

NaiveMapSurface CreateMapSurface(int num_dates, int num_strikes) {
    NaiveMapSurface surface;
    cdr::DateType today = cdr::Today();
    for (int d = 1; d <= num_dates; ++d) {
        cdr::DateType expiry = cdr::AddDays(today, d * 30);
        for (int s = 0; s < num_strikes; ++s) {
            surface.data[expiry][80.0 + s * 2.0] = 0.20 + (s * 0.001);
        }
    }
    return surface;
}

struct Query {
    cdr::DateType date;
    double strike;
};

std::vector<Query> GenerateQueries(int count, int num_dates) {
    std::vector<Query> queries;
    queries.reserve(count);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> d_dist(1, num_dates * 30);
    std::uniform_real_distribution<> s_dist(70.0, 150.0);
    for (int i = 0; i < count; ++i) {
        queries.push_back({cdr::AddDays(cdr::Today(), d_dist(gen)), s_dist(gen)});
    }
    return queries;
}

}  // anonymous namespace

static void BM_Flat_Random_Mid(benchmark::State& state) {
    auto surface = CreateFlatSurface(20, 100);
    auto queries = GenerateQueries(1000, 20);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Flat_Random_Mid)->Unit(benchmark::kNanosecond);

static void BM_Map_Random_Mid(benchmark::State& state) {
    auto surface = CreateMapSurface(20, 100);
    auto queries = GenerateQueries(1000, 20);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Map_Random_Mid)->Unit(benchmark::kNanosecond);

static void BM_Flat_ColdCache(benchmark::State& state) {
    auto surface = CreateFlatSurface(50, 200);
    auto queries = GenerateQueries(100000, 50);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 100000].date, queries[i % 100000].strike));
        i++;
    }
}
BENCHMARK(BM_Flat_ColdCache)->Unit(benchmark::kNanosecond);

static void BM_Flat_SmileSweep(benchmark::State& state) {
    auto surface = CreateFlatSurface(12, 100);
    cdr::DateType expiry = cdr::AddDays(cdr::Today(), 30);
    double strike = 60.0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(expiry, strike));
        strike += 0.01;
        if (strike > 180.0) strike = 60.0;
    }
}
BENCHMARK(BM_Flat_SmileSweep)->Unit(benchmark::kNanosecond);

static void BM_Flat_Extreme_Large(benchmark::State& state) {
    auto surface = CreateFlatSurface(500, 1000);
    auto queries = GenerateQueries(1000, 500);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Flat_Extreme_Large)->Unit(benchmark::kNanosecond);

static void BM_Map_Extreme_Large(benchmark::State& state) {
    auto surface = CreateMapSurface(500, 1000);
    auto queries = GenerateQueries(1000, 500);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Map_Extreme_Large)->Unit(benchmark::kNanosecond);

static void BM_Flat_Huge(benchmark::State& state) {
    auto surface = CreateFlatSurface(1000, 2000);
    auto queries = GenerateQueries(1000, 1000);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Flat_Huge)->Unit(benchmark::kNanosecond);

static void BM_Map_Huge(benchmark::State& state) {
    auto surface = CreateMapSurface(1000, 2000);
    auto queries = GenerateQueries(1000, 1000);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Map_Huge)->Unit(benchmark::kNanosecond);

static void BM_Flat_Monster(benchmark::State& state) {
    auto surface = CreateFlatSurface(2000, 4000);
    auto queries = GenerateQueries(1000, 2000);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Flat_Monster)->Unit(benchmark::kNanosecond);

static void BM_Map_Monster(benchmark::State& state) {
    auto surface = CreateMapSurface(2000, 4000);
    auto queries = GenerateQueries(1000, 2000);
    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(surface.Volatility(queries[i % 1000].date, queries[i % 1000].strike));
        i++;
    }
}
BENCHMARK(BM_Map_Monster)->Unit(benchmark::kNanosecond);

static void BM_Snapshot_Update_Latency(benchmark::State& state) {
    cdr::DateType today = cdr::Today();
    for (auto _ : state) {
        state.PauseTiming();
        cdr::VolatilitySurfaceProvider provider{today};
        for (int d = 1; d <= 12; ++d) {
            for (int s = 0; s < 100; ++s) {
                provider.AddPillar(cdr::AddDays(today, d * 30), 80.0 + s * 2, 0.2).OrCrashProgram();
            }
        }
        state.ResumeTiming();
        benchmark::DoNotOptimize(provider.UpdateSnapshot());
    }
}
BENCHMARK(BM_Snapshot_Update_Latency)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();