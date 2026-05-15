#include <gtest/gtest.h>
#include <cdr/options/diffprog_pricing.h>
#include <cdr/options/helpers.h>

using namespace cdr;

struct PricingTestCase {
    const char* name;
    f64 S, K, rd, rf, sigma, T;
    OptionType type;
};

class PricingConsistencyTest : public ::testing::TestWithParam<PricingTestCase> {};

TEST_P(PricingConsistencyTest, AutoDiffMatchesAnalytical) {
    const auto& tc = GetParam();
    const f64 eps = 1e-13;

    // 1. Delta
    f64 delta_analyt = FxOptionDelta(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    f64 delta_ad     = FxOptionDeltaAD(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    EXPECT_NEAR(delta_analyt, delta_ad, eps)
        << "Delta mismatch in test case: " << tc.name;

    // 2. Vega
    f64 vega_analyt = FxOptionVega(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    f64 vega_ad     = FxOptionVegaAD(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    EXPECT_NEAR(vega_analyt, vega_ad, eps)
        << "Vega mismatch in test case: " << tc.name;

    // 3. Rho
    f64 rho_analyt = FxOptionRho(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    f64 rho_ad     = FxOptionRhoAD(tc.S, tc.K, tc.rd, tc.rf, tc.sigma, tc.T, tc.type);
    EXPECT_NEAR(rho_analyt, rho_ad, eps)
        << "Rho mismatch in test case: " << tc.name;
}

INSTANTIATE_TEST_SUITE_P(
    OptionGreeks,
    PricingConsistencyTest,
    ::testing::Values(
        PricingTestCase{"ATM_Call", 1.10, 1.10, 0.03, 0.01, 0.15, 0.5, OptionType::CALL},
        PricingTestCase{"ITM_Call", 1.50, 1.10, 0.03, 0.01, 0.20, 0.25, OptionType::CALL},
        PricingTestCase{"OTM_Put", 1.20, 1.00, 0.02, 0.05, 0.30, 1.0, OptionType::PUT},
        PricingTestCase{"Short_Exp", 1.00, 1.00, 0.01, 0.01, 0.10, 0.01, OptionType::CALL}
    )
);
