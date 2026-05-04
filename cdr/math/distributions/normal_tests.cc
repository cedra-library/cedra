#include <vector>
#include <utility>

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
        ASSERT_NEAR(cdr::NormalCDF(x), target, precision);
    }
}

}  // anonymous namespace
