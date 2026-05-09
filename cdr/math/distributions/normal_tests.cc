#include <vector>
#include <utility>
#include <random>
#include <ranges>

#include <gtest/gtest.h>

#include <cdr/math/distributions/normal.h>

using cdr::Percent;

namespace {

TEST(Normal, Correctness) {
    constexpr f64 precision = 0.00005;
    std::vector<std::pair<f64, f64>> test_cases {
        {-3.9, 0.0},
        {-3.05, 0.0011},
        {-2.4, 0.0082},
        {-0.19, 0.4247},
        {0.0, 0.5},
        {0.1, 0.5398},
        {0.54, 0.7054},
        {2.71, 0.9966},
        {3.92, 1.0},
    };

    for (const auto& [x, target] : test_cases) {
        ASSERT_NEAR(cdr::NormalCDF(x), target, precision) << "x: " << x;
    }
    for (const auto& [x, target] : test_cases) {
        ASSERT_NEAR(cdr::NormalCDFStdLib(x), target, precision) << "x: " << x;
    }
}

TEST(Normal, Compare) {
    constexpr f64 precision = 0.000001;

    std::mt19937 gen(88);
    std::uniform_real_distribution<f64> uniform(-4., 4.);

    for (auto _ : std::views::iota(0, 100)) {
        f64 x = uniform(gen);
        ASSERT_NEAR(cdr::NormalCDFStdLib(x), cdr::NormalCDFHartAlgorithm(x), precision) << "x: " << x;
    }
}

}  // anonymous namespace
