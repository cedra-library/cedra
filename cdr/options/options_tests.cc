#include <gtest/gtest.h>

#include <cdr/options/helpers.h>


namespace {

TEST(Options, VolatilityFromPrice) {
    f64 S = 100;
    f64 K = 100;
    f64 rd = 0.01;
    f64 rf = 0.02;
    f64 T = 1;
    cdr::OptionType type = cdr::OptionType::CALL;

    for (f64 sigma = 0.1; sigma <= 0.5; sigma += 0.1) {
        f64 price = cdr::FxOptionPrice(S, K, rd, rf, sigma, T, type);
        f64 implied_vol = cdr::FxOptionSigmaFromPrice(S, K, rd, rf, T, type, price);
        ASSERT_NEAR(implied_vol, sigma, 1e-4);
    }
}

}  // anonymous namespace
