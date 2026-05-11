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
    // 1. Контекст и даты
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("USD", day(1)/January/year(2026))
        ("EUR", day(1)/January/year(2026));

    DateType today = day(1) / January / year(2026);
    cdr::MarketContext context(std::move(hs), today);

    // 2. Кривые (нужны для расчета дельты)
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

    // 3. Провайдер поверхности
    cdr::VolatilitySurfaceProvider provider(today);

    // Добавляем страйки (создаем явную V-образную улыбку)
    provider.AddPillar(expiry, 0.90, 0.25).OrCrashProgram();
    provider.AddPillar(expiry, 1.00, 0.18).OrCrashProgram();
    provider.AddPillar(expiry, 1.10, 0.15).OrCrashProgram();
    provider.AddPillar(expiry, 1.20, 0.19).OrCrashProgram();
    provider.AddPillar(expiry, 1.30, 0.24).OrCrashProgram();

    // Добавляем опорные дельты (Pillar Deltas)
    const std::vector<double> pillar_deltas = {-0.35, -0.25, -0.15, 0.15, 0.25, 0.35};
    for (double d : pillar_deltas) {
        provider.AddPillarDelta(d).OrCrashProgram();
    }

    // Сборка снэпшота
    ASSERT_TRUE(provider.UpdateSnapshot(spot, *domestic, *foreign).Succeed());
    auto surface = provider.ProvideSnapshot().Value();

    // 4. Проверка консистентности на узлах дельт
    for (double target_delta : pillar_deltas) {
        // Получаем волатильность напрямую через интерполятор по дельтам
        auto vol_by_delta_res = surface.VolatilityByDelta(expiry, target_delta);
        ASSERT_TRUE(vol_by_delta_res.Succeed()) << "Failed for delta: " << target_delta;
        double vol_from_delta_space = vol_by_delta_res.Value();

        // Проверяем, соответствует ли это волатильности в пространстве страйков.
        // Для этого найдем страйк K, при котором Delta(K, Vol_Surface(K)) == target_delta.
        cdr::OptionType type = (target_delta > 0) ? cdr::OptionType::CALL : cdr::OptionType::PUT;

        auto objective = [&](double K) {
            double v = surface.Volatility(expiry, K).Value();
            return cdr::FxOptionDelta(spot, K, rd_rate, rf_rate, v, T, type) - target_delta;
        };

        // Решаем уравнение относительно K
        double solved_K = cdr::FindRoot(objective, 0.5, 2.0, spot).value_or(spot);
        double vol_from_strike_space = surface.Volatility(expiry, solved_K).Value();

        // Значения должны совпадать (допускаем небольшую погрешность из-за численного поиска корня)
        EXPECT_NEAR(vol_from_delta_space, vol_from_strike_space, 1e-6)
            << "Inconsistency between delta-space and strike-space at delta=" << target_delta;
    }

    // 5. Проверка интерполяции в "пустом" месте (между узлами)
    double mid_delta = 0.20; // Между 0.15 и 0.25
    auto vol_mid = surface.VolatilityByDelta(expiry, mid_delta);
    ASSERT_TRUE(vol_mid.Succeed());
    EXPECT_GT(vol_mid.Value(), 0.15); // Проверка на здравый смысл (волатильность положительна)

    // 6. Проверка экстраполяции (должна возвращать ошибку)
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, 0.95).Failed());
    EXPECT_TRUE(surface.VolatilityByDelta(expiry, -0.95).Failed());
}
