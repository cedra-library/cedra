#include <gtest/gtest.h>

#include <cdr/options/interpolation/cubic_spline.h>

#include <vector>

using cdr::Percent;

using Interpolator = cdr::CubicSplineInterpolator;

namespace {

struct StateOwner {
    void* memory;
    size_t size;

    StateOwner(size_t n) {
        size = Interpolator::StateRequiredMemorySize(n);
        size_t alignment = Interpolator::StateRequiredMemoryAlignment(n);
        memory = std::aligned_alloc(alignment, size);
    }

    ~StateOwner() {
        std::free(memory);
    }
};

TEST(CubicSplineInterpolator, Simple) {
    std::vector<std::pair<std::vector<f64>, std::vector<f64>>> test_cases {
        {{0, 1}, {0, 1}},
        {{0, 1, 2}, {0, 1, 4}},
        {{0, 1, 2}, {1, 2, 3}},
        {{0, 1, 2}, {1, 3, 2}},
        {{0, 1, 2}, {3, 2, 1}},
    };

    for (const auto& [x_values, y_values] : test_cases) {
        size_t n = x_values.size();
        StateOwner state(n);
        ASSERT_TRUE(Interpolator::InitState(state.memory, x_values, y_values).Succeed());

        for (size_t i = 0; i < x_values.size(); i++) {
            f64 interpolated = Interpolator::Evaluate(state.memory, x_values, x_values[i]).OrCrashProgram();
            ASSERT_NEAR(interpolated, y_values[i], 1e-6);
        }
        Interpolator::DestroyState(state.memory, n);
    }
}

TEST(CubicSplineInterpolator, Linear) {
    auto gen = [](f64 a, f64 b) {
        return [a, b](f64 x) {
            return a * x + b;
        };
    };

    std::vector<std::tuple<f64, f64>> test_cases {
        {2, 3},
        {2, 5},
        {2, 1},
        {2, -1},
    };

    constexpr size_t kNumValues = 5;

    for (const auto& [a, b] : test_cases) {
        std::vector<f64> x_values(kNumValues);
        std::vector<f64> y_values(kNumValues);
        auto f = gen(a, b);

        for (size_t i = 0; i < kNumValues; i++) {
            x_values[i] = static_cast<f64>(i);
            y_values[i] = f(x_values[i]);
        }

        StateOwner state(x_values.size());
        ASSERT_TRUE(Interpolator::InitState(state.memory, x_values, y_values).Succeed());

        constexpr f64 step = 0.1;
        for (f64 x = x_values.front(); x <= x_values.back(); x += step) {
            f64 interpolated = Interpolator::Evaluate(state.memory, x_values, x).OrCrashProgram();
            ASSERT_NEAR(interpolated, f(x), 1e-6);
        }
        Interpolator::DestroyState(state.memory, x_values.size());
    }
}

TEST(CubicSplineInterpolator, Sin) {
    std::vector<f64> x_values {0, 1, 2, 3, 4, 5};
    std::vector<f64> y_values(x_values.size());
    for (size_t i = 0; i < x_values.size(); i++) {
        y_values[i] = std::sin(x_values[i]);
    }

    StateOwner state(x_values.size());
    ASSERT_TRUE(Interpolator::InitState(state.memory, x_values, y_values).Succeed());

    constexpr f64 step = 0.1;
    for (f64 x = x_values.front(); x <= x_values.back(); x += step) {
        f64 interpolated = Interpolator::Evaluate(state.memory, x_values, x).OrCrashProgram();
        ASSERT_NEAR(interpolated, std::sin(x), 0.1);
    }
    Interpolator::DestroyState(state.memory, x_values.size());
}

}
