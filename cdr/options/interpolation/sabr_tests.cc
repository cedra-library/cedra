#include <gtest/gtest.h>
#include <cdr/options/interpolation/sabr.h>
#include <vector>
#include <random>

using namespace cdr;

class SABRInterpolatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Настраиваем базовые параметры для тестов
        params.F = 100.0;
        params.T = 1.0;
        params.alpha = 0.2;
        params.rho = -0.3;
        params.nu = 0.4;
    }

    SABRInterpolator::SabrParameters params;
};


TEST_F(SABRInterpolatorTest, AtTheMoneyVolatility) {
    f64 strike = params.F;
    auto result = SABRInterpolator::Evaluate(&params, {}, strike);

    ASSERT_TRUE(result.Succeed());

    EXPECT_GT(result.Value(), 0.1);
    EXPECT_LT(result.Value(), 0.3);
}

TEST_F(SABRInterpolatorTest, CalibrationTest) {
    std::vector<f64> strikes = {80.0, 90.0, 100.0, 110.0, 120.0};
    std::vector<f64> market_vols;

    for (f64 k : strikes) {
        market_vols.push_back(SABRInterpolator::Evaluate(&params, {}, k).Value());
    }

    SABRInterpolator::SabrParameters calibrated_params;
    calibrated_params.F = params.F;
    calibrated_params.T = params.T;

    auto init_res = SABRInterpolator::InitState(&calibrated_params, strikes, market_vols);
    ASSERT_TRUE(init_res.Succeed());

    EXPECT_NEAR(calibrated_params.alpha, params.alpha, 1e-4);
    EXPECT_NEAR(calibrated_params.rho, params.rho, 1e-3);
    EXPECT_NEAR(calibrated_params.nu, params.nu, 1e-3);
}

TEST_F(SABRInterpolatorTest, InvalidInputs) {
    auto res_neg = SABRInterpolator::Evaluate(&params, {}, -10.0);
    EXPECT_EQ(res_neg.Value(), 0.0);

    auto res_high = SABRInterpolator::Evaluate(&params, {}, 1000.0);
    EXPECT_TRUE(res_high.Succeed());
    EXPECT_GT(res_high.Value(), 0.0);
}


TEST_F(SABRInterpolatorTest, ContinuityAroundATM) {
    const f64 eps = 1e-8;

    auto left  = SABRInterpolator::Evaluate(&params, {}, params.F - eps);
    auto atm   = SABRInterpolator::Evaluate(&params, {}, params.F);
    auto right = SABRInterpolator::Evaluate(&params, {}, params.F + eps);

    ASSERT_TRUE(left.Succeed());
    ASSERT_TRUE(atm.Succeed());
    ASSERT_TRUE(right.Succeed());

    EXPECT_NEAR(left.Value(), atm.Value(), 1e-8);
    EXPECT_NEAR(right.Value(), atm.Value(), 1e-8);
}

TEST_F(SABRInterpolatorTest, SmileShape) {
    auto low  = SABRInterpolator::Evaluate(&params, {}, 80.0);
    auto atm  = SABRInterpolator::Evaluate(&params, {}, 100.0);
    auto high = SABRInterpolator::Evaluate(&params, {}, 120.0);

    ASSERT_TRUE(low.Succeed());
    ASSERT_TRUE(atm.Succeed());
    ASSERT_TRUE(high.Succeed());

    EXPECT_GT(low.Value(), atm.Value());
    EXPECT_LT(high.Value(), low.Value());
}


TEST_F(SABRInterpolatorTest, WideStrikeRangeProducesFiniteVols) {
    for (f64 strike = 1.0; strike <= 500.0; strike += 1.0) {
        auto res = SABRInterpolator::Evaluate(&params, {}, strike);

        ASSERT_TRUE(res.Succeed());

        EXPECT_TRUE(std::isfinite(res.Value()));
        EXPECT_GT(res.Value(), 0.0);
    }
}


TEST_F(SABRInterpolatorTest, ExtremeRho) {
    params.rho = 0.999;

    auto res1 = SABRInterpolator::Evaluate(&params, {}, 80.0);
    auto res2 = SABRInterpolator::Evaluate(&params, {}, 120.0);

    ASSERT_TRUE(res1.Succeed());
    ASSERT_TRUE(res2.Succeed());

    EXPECT_TRUE(std::isfinite(res1.Value()));
    EXPECT_TRUE(std::isfinite(res2.Value()));
}

TEST_F(SABRInterpolatorTest, CalibrationReproducesMarketSmile) {
    std::vector<f64> strikes = {80, 90, 100, 110, 120};
    std::vector<f64> market_vols;

    for (f64 k : strikes) {
        market_vols.push_back(
            SABRInterpolator::Evaluate(&params, {}, k).Value()
        );
    }

    SABRInterpolator::SabrParameters calibrated;
    calibrated.F = params.F;
    calibrated.T = params.T;

    auto init_res =
        SABRInterpolator::InitState(&calibrated, strikes, market_vols);

    ASSERT_TRUE(init_res.Succeed());

    for (size_t i = 0; i < strikes.size(); ++i) {
        auto vol =
            SABRInterpolator::Evaluate(&calibrated, {}, strikes[i]);

        ASSERT_TRUE(vol.Succeed());

        EXPECT_NEAR(
            vol.Value(),
            market_vols[i],
            1e-7
        );
    }
}


TEST_F(SABRInterpolatorTest, CalibrationWithNoise) {
    std::vector<f64> strikes = {80, 90, 100, 110, 120};

    std::vector<f64> vols;

    for (f64 k : strikes) {
        auto vol =
            SABRInterpolator::Evaluate(&params, {}, k).Value();

        vols.push_back(vol + 1e-4);
    }

    SABRInterpolator::SabrParameters calibrated;
    calibrated.F = params.F;
    calibrated.T = params.T;

    auto res =
        SABRInterpolator::InitState(&calibrated, strikes, vols);

    ASSERT_TRUE(res.Succeed());

    for (size_t i = 0; i < strikes.size(); ++i) {
        auto model =
            SABRInterpolator::Evaluate(&calibrated, {}, strikes[i]);

        EXPECT_NEAR(model.Value(), vols[i], 1e-3);
    }
}

TEST(SABRFuzzTest, RandomParameters) {
    std::mt19937 rng(42);

    std::uniform_real_distribution<f64> alpha_dist(0.01, 1.0);
    std::uniform_real_distribution<f64> rho_dist(-0.99, 0.99);
    std::uniform_real_distribution<f64> nu_dist(0.01, 2.0);
    std::uniform_real_distribution<f64> strike_dist(1.0, 300.0);

    for (int i = 0; i < 10000; ++i) {
        SABRInterpolator::SabrParameters p;

        p.alpha = alpha_dist(rng);
        p.rho = rho_dist(rng);
        p.nu = nu_dist(rng);
        p.F = 100.0;
        p.T = 1.0;

        auto res =
            SABRInterpolator::Evaluate(&p, {}, strike_dist(rng));

        ASSERT_TRUE(res.Succeed());

        EXPECT_TRUE(std::isfinite(res.Value()));
        EXPECT_GT(res.Value(), 0.0);
    }
}
