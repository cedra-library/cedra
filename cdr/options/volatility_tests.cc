
#include <cdr/calendar/holiday_storage.h>
#include <cdr/curve/curve.h>
#include <cdr/curve/interpolation/linear.h>
#include <cdr/options/helpers.h>  // Для FxOptionDelta
#include <cdr/options/volatility.h>
#include <gtest/gtest.h>

#include <cmath>

using namespace std::chrono;
using cdr::Percent;

TEST(VolatilitySurface, DeltaInterpolationConsistency) {
    cdr::HolidayStorage hs;
    hs.StaticInit()("USD", day(1) / January / year(2026))("EUR", day(1) / January / year(2026));

    DateType today = day(1) / January / year(2026);
    cdr::MarketContext context(std::move(hs), today);

    const double rd_rate = 0.05;
    const double rf_rate = 0.02;

    auto domestic = cdr::CurveBuilder(context)
                        .Jurisdiction("USD")
                        .Add(day(1) / February / year(2026), Percent::FromPercentage(rd_rate * 100))
                        .FromPoints();
    auto foreign = cdr::CurveBuilder(context)
                       .Jurisdiction("EUR")
                       .Add(day(1) / February / year(2026), Percent::FromPercentage(rf_rate * 100))
                       .FromPoints();

    constexpr double spot = 1.10;
    const DateType expiry = day(1) / February / year(2026);
    const double T = cdr::Period{today, expiry}.Act365();

    cdr::VolatilitySurfaceProvider provider(today);

    provider.AddPillar(expiry, 0.90, 0.25).OrCrashProgram();
    provider.AddPillar(expiry, 1.00, 0.18).OrCrashProgram();
    provider.AddPillar(expiry, 1.10, 0.15).OrCrashProgram();
    provider.AddPillar(expiry, 1.20, 0.19).OrCrashProgram();
    provider.AddPillar(expiry, 1.30, 0.24).OrCrashProgram();

    const std::vector<double> pillar_deltas = {-0.35, -0.25, -0.15, 0.15, 0.25, 0.35};
    for (double d : pillar_deltas) {
        provider.AddPillarDelta(d).OrCrashProgram();
    }

    ASSERT_TRUE(provider.UpdateSnapshot(spot, *domestic, *foreign).Succeed());
    auto surface = provider.ProvideSnapshot().Value();

    for (double target_delta : pillar_deltas) {
        auto vol_by_delta_res = surface.VolatilityByDelta(expiry, target_delta);
        ASSERT_TRUE(vol_by_delta_res.Succeed()) << "Failed for delta: " << target_delta;
        double vol_from_delta_space = vol_by_delta_res.Value();

        cdr::OptionType type = (target_delta > 0) ? cdr::OptionType::CALL : cdr::OptionType::PUT;

        auto objective = [&](double K) {
            double v = surface.Volatility(expiry, K).Value();
            return cdr::FxOptionDelta(spot, K, rd_rate, rf_rate, v, T, type) - target_delta;
        };

        double solved_K = spot;
        auto root_search = cdr::FindRoot(objective, surface.Strikes().front(), surface.Strikes().back(), spot);
        if (root_search.Succeed()) {
            solved_K = root_search.Value();
        }

        double vol_from_strike_space = surface.Volatility(expiry, solved_K).Value();

        EXPECT_NEAR(vol_from_delta_space, vol_from_strike_space, 1e-6)
            << "Inconsistency between delta-space and strike-space at delta=" << target_delta;
    }

    double mid_delta = 0.20;
    auto vol_mid = surface.VolatilityByDelta(expiry, mid_delta);
    ASSERT_TRUE(vol_mid.Succeed());
    EXPECT_GT(vol_mid.Value(), 0.15);  // Проверка на здравый смысл (волатильность положительна)

    EXPECT_TRUE(surface.VolatilityByDelta(expiry, 0.95).Failed());
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, -0.95).Failed());
}

TEST(SABRVolatilitySurface, DeltaInterpolationConsistency) {
    cdr::HolidayStorage hs;
    hs.StaticInit()("USD", day(1) / January / year(2026))("EUR", day(1) / January / year(2026));

    DateType today = day(1) / January / year(2026);
    cdr::MarketContext context(std::move(hs), today);

    constexpr double rd_rate = 0.05;
    constexpr double rf_rate = 0.02;

    auto domestic = cdr::CurveBuilder(context)
                        .Jurisdiction("USD")
                        .Add(day(1) / February / year(2026), Percent::FromPercentage(rd_rate * 100.0))
                        .FromPoints();

    auto foreign = cdr::CurveBuilder(context)
                       .Jurisdiction("EUR")
                       .Add(day(1) / February / year(2026), Percent::FromPercentage(rf_rate * 100.0))
                       .FromPoints();

    constexpr double spot = 1.10;

    const DateType expiry = day(1) / February / year(2026);
    const double T = cdr::Period{today, expiry}.Act365();

    //
    // Synthetic SABR smile
    //
    constexpr double alpha = 0.15;
    constexpr double rho = -0.35;
    constexpr double nu = 0.80;

    const double F = spot * std::exp((rd_rate - rf_rate) * T);

    std::vector<double> strikes{0.85, 0.95, 1.00, 1.05, 1.10, 1.15, 1.20, 1.30};

    cdr::VolatilitySurfaceProvider<cdr::SABRInterpolator> provider(today);

    for (double K : strikes) {
        const double vol = cdr::SABRInterpolator::CalculateHagan<double>(K, F, T, alpha, rho, nu);

        provider.AddPillar(expiry, K, vol).OrCrashProgram();
    }

    const std::vector<double> pillar_deltas{-0.35, -0.25, -0.15, 0.15, 0.25, 0.35};

    for (double d : pillar_deltas) {
        provider.AddPillarDelta(d).OrCrashProgram();
    }

    ASSERT_TRUE(provider.UpdateSnapshot(spot, *domestic, *foreign).Succeed());

    auto surface = provider.ProvideSnapshot().Value();

    //
    // Delta-space consistency:
    //
    // VolatilityByDelta(delta) == Volatility(K(delta))
    //
    for (double target_delta : pillar_deltas) {
        auto vol_by_delta_res = surface.VolatilityByDelta(expiry, target_delta);

        ASSERT_TRUE(vol_by_delta_res.Succeed()) << "Failed for delta: " << target_delta;

        const double vol_from_delta_space = vol_by_delta_res.Value();
        const cdr::OptionType type = (target_delta > 0.0) ? cdr::OptionType::CALL : cdr::OptionType::PUT;

        auto objective = [&](double K) {
            auto vol_res = surface.Volatility(expiry, K);
            EXPECT_TRUE(vol_res.Succeed());
            const double vol = vol_res.Value();
            return cdr::FxOptionDelta(spot, K, rd_rate, rf_rate, vol, T, type) - target_delta;
        };

        auto root_search = cdr::FindRoot(objective, strikes.front(), strikes.back(), F);
        ASSERT_TRUE(root_search.Succeed()) << "Could not invert delta=" << target_delta;

        const double solved_K = root_search.Value();
        auto vol_from_strike_res = surface.Volatility(expiry, solved_K);
        ASSERT_TRUE(vol_from_strike_res.Succeed());

        const double vol_from_strike_space = vol_from_strike_res.Value();
        EXPECT_NEAR(vol_from_delta_space, vol_from_strike_space, 1e-6)
            << "Delta/Strike inconsistency at delta=" << target_delta;
    }

    //
    // Mid delta sanity check
    //

    constexpr double mid_delta = 0.20;

    auto mid_vol = surface.VolatilityByDelta(expiry, mid_delta);

    ASSERT_TRUE(mid_vol.Succeed());

    EXPECT_TRUE(std::isfinite(mid_vol.Value()));
    EXPECT_GT(mid_vol.Value(), 0.0);

    //
    // Extrapolation checks
    //
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, 0.95).Failed());
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, -0.95).Failed());
}


