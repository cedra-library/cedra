#include <utility>
#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include <cdr/math/newton_raphson/newton_raphson.h>

using cdr::Percent;

namespace {

const f64 kTol = std::exp2(-16.);

TEST(FindRoot, FindsRootOfLinearFunction) {
    auto f = [](f64 x) -> f64 {
        return 2.0 * x - 6.0;
    };

    auto root = cdr::FindRoot(f, 0.0, 10.0, std::nullopt);

    ASSERT_TRUE(root.has_value());
    EXPECT_NEAR(*root, 3.0, kTol);
    EXPECT_NEAR(f(*root), 0.0, kTol);
}

TEST(FindRoot, FindsRootOfQuadraticFunction) {
    auto f = [](f64 x) -> f64 {
        return x * x - 4.0;
    };

    auto root = cdr::FindRoot(f, 0.0, 10.0, std::nullopt);

    ASSERT_TRUE(root.has_value());
    EXPECT_NEAR(*root, 2.0, 1e-7);
    EXPECT_NEAR(f(*root), 0.0, kTol);
}

TEST(FindRoot, RespectsProvidedStartPoint) {
    auto f = [](f64 x) -> f64 {
        return x * x - 4.0;
    };

    auto root = cdr::FindRoot(f, -10.0, 0.0, -3.0);

    ASSERT_TRUE(root.has_value());
    EXPECT_NEAR(*root, -2.0, 1e-7);
    EXPECT_NEAR(f(*root), 0.0, kTol);
}

TEST(FindRoot, FindsRootOfSin) {
    auto f = [](f64 x) -> f64 {
        return std::sin(x);
    };

    auto root = cdr::FindRoot(f, 3.0, 4.0, std::nullopt);

    ASSERT_TRUE(root.has_value());
    EXPECT_NEAR(*root, std::numbers::pi, kTol);
    EXPECT_NEAR(f(*root), 0.0, kTol);
}

}  // anonymous namespace
