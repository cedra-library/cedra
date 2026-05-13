// SURFACE IS CHANGING

#include <cdr/calendar/holiday_storage.h>
#include <cdr/curve/curve.h>
#include <cdr/curve/interpolation/linear.h>
#include <cdr/options/volatility.h>
#include <cdr/options/helpers.h> // Для FxOptionDelta
#include <gtest/gtest.h>
#include <cmath>

using namespace std::chrono;
using cdr::Percent;

TEST(VolatilitySurface, DeltaInterpolationConsistency) {

    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("USD", day(1)/January/year(2026))
        ("EUR", day(1)/January/year(2026));

    DateType today = day(1) / January / year(2026);
    cdr::MarketContext context(std::move(hs), today);

    const double rd_rate = 0.05;
    const double rf_rate = 0.02;

    auto domestic = cdr::CurveBuilder(context).Jurisdiction("USD")
                        .Add(day(1)/February/year(2026), Percent::FromPercentage(rd_rate * 100))
                        .FromPoints();
    auto foreign = cdr::CurveBuilder(context).Jurisdiction("EUR")
                       .Add(day(1)/February/year(2026), Percent::FromPercentage(rf_rate * 100))
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
    EXPECT_GT(vol_mid.Value(), 0.15); // Проверка на здравый смысл (волатильность положительна)

    EXPECT_TRUE(surface.VolatilityByDelta(expiry, 0.95).Failed());
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, -0.95).Failed());
}
