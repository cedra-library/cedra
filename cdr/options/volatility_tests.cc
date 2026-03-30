#include <gtest/gtest.h>

#include <cdr/options/volatility.h>
#include <cdr/calendar/date.h>

TEST(VolatilityTest, SanityCheck) {
    cdr::DateType kToday = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{kToday};

    // Populate pillars for a single expiration date
    provider.AddPillar(cdr::NextDay(kToday), 100, 0.01).OrCrashProgram();
    provider.AddPillar(cdr::NextDay(kToday), 110, 0.11).OrCrashProgram();
    provider.AddPillar(cdr::NextDay(kToday), 115, 0.12).OrCrashProgram();
    provider.AddPillar(cdr::NextDay(kToday), 120, 0.14).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_provided = provider.ProvideSnapshot();
    ASSERT_TRUE(snapshot_provided.Succeed()) << "Snapshot must be provided";
    cdr::VolatilitySurface surface = std::move(snapshot_provided).Value();

    // Verify exact matches at grid nodes
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 100), 0.01);
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 110), 0.11);
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 115), 0.12);
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 120), 0.14);

    // Verify horizontal extrapolation (flat clamping)
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 125), 0.14);
    ASSERT_EQ(surface.Volatility(cdr::NextDay(kToday), 90), 0.01);

    // Verify linear interpolation between strikes
    ASSERT_NEAR(surface.Volatility(cdr::NextDay(kToday), 105), 0.06, 1e-7);
}

TEST(VolatilityMathTest, GridInterpolationAndExtrapolation) {
    cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{today};

    // Define two distinct expiration dates: T1 (+10d) and T2 (+20d)
    cdr::DateType t1 = cdr::AddDays(today, 10);
    cdr::DateType t2 = cdr::AddDays(today, 20);
    cdr::DateType t_mid = cdr::AddDays(today, 15);

    // Setup T1: 100 -> 10%, 110 -> 20%
    provider.AddPillar(t1, 100.0, 0.10).OrCrashProgram();
    provider.AddPillar(t1, 110.0, 0.20).OrCrashProgram();
    // Setup T2: 100 -> 20%, 110 -> 30%
    provider.AddPillar(t2, 100.0, 0.20).OrCrashProgram();
    provider.AddPillar(t2, 110.0, 0.30).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_res = provider.ProvideSnapshot();
    ASSERT_TRUE(snapshot_res.Succeed());
    const auto& surface = snapshot_res.Value();

    // 1. Verify exact grid nodes
    EXPECT_NEAR(surface.Volatility(t1, 100.0), 0.10, 1e-7);
    EXPECT_NEAR(surface.Volatility(t1, 110.0), 0.20, 1e-7);
    EXPECT_NEAR(surface.Volatility(t2, 100.0), 0.20, 1e-7);
    EXPECT_NEAR(surface.Volatility(t2, 110.0), 0.30, 1e-7);

    // 2. Verify horizontal interpolation (by strike)
    EXPECT_NEAR(surface.Volatility(t1, 105.0), 0.15, 1e-7);
    EXPECT_NEAR(surface.Volatility(t2, 105.0), 0.25, 1e-7);

    // 3. Verify vertical interpolation (by time)
    EXPECT_NEAR(surface.Volatility(t_mid, 100.0), 0.15, 1e-7);
    EXPECT_NEAR(surface.Volatility(t_mid, 110.0), 0.25, 1e-7);

    // 4. Verify bilinear interpolation at the center of the grid cell
    EXPECT_NEAR(surface.Volatility(t_mid, 105.0), 0.20, 1e-7);

    // 5. Verify clamping (boundary extrapolation)
    // Strike clamping (lower/upper)
    EXPECT_NEAR(surface.Volatility(t1, 50.0), 0.10, 1e-7);
    EXPECT_NEAR(surface.Volatility(t1, 150.0), 0.20, 1e-7);
    // Time clamping (front/back)
    EXPECT_NEAR(surface.Volatility(cdr::AddDays(today, 5), 100.0), 0.10, 1e-7);
    EXPECT_NEAR(surface.Volatility(cdr::AddDays(today, 30), 100.0), 0.20, 1e-7);
}

TEST(VolatilityMathTest, SparseGridInterpolation) {
    cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{today};

    cdr::DateType t1 = cdr::AddDays(today, 10);
    cdr::DateType t2 = cdr::AddDays(today, 20);

    // T1: Define pillars for strikes 100 and 120 only
    provider.AddPillar(t1, 100.0, 0.10).OrCrashProgram();
    provider.AddPillar(t1, 120.0, 0.20).OrCrashProgram();

    // T2: Define pillars for entirely different strikes: 110 and 130
    provider.AddPillar(t2, 110.0, 0.25).OrCrashProgram();
    provider.AddPillar(t2, 130.0, 0.30).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_res = provider.ProvideSnapshot();
    ASSERT_TRUE(snapshot_res.Succeed());
    const auto& surface = snapshot_res.Value();

    // 1. Verify original pillars remain intact
    EXPECT_NEAR(surface.Volatility(t1, 100.0), 0.10, 1e-7);
    EXPECT_NEAR(surface.Volatility(t1, 120.0), 0.20, 1e-7);
    EXPECT_NEAR(surface.Volatility(t2, 110.0), 0.25, 1e-7);
    EXPECT_NEAR(surface.Volatility(t2, 130.0), 0.30, 1e-7);

    // 2. Verify strike interpolation for missing pillars in T1
    // Strike 110 on T1 is interpolated between 100 and 120 (Target: 0.15)
    EXPECT_NEAR(surface.Volatility(t1, 110.0), 0.15, 1e-7);

    // 3. Verify strike interpolation for missing pillars in T2
    // Strike 120 on T2 is interpolated between 110 and 130 (Target: 0.275)
    EXPECT_NEAR(surface.Volatility(t2, 120.0), 0.275, 1e-7);

    // 4. Verify cross-layer clamping
    // On T1, strike 130 is outside local bounds (max 120) -> returns 0.20
    EXPECT_NEAR(surface.Volatility(t1, 130.0), 0.20, 1e-7);
    // On T2, strike 100 is outside local bounds (min 110) -> returns 0.25
    EXPECT_NEAR(surface.Volatility(t2, 100.0), 0.25, 1e-7);

    // 5. Verify bilinear interpolation on a synthetic point (T_mid, Strike 115)
    // T1(115) = 0.175, T2(115) = 0.2625 -> Average = 0.21875
    cdr::DateType t_mid = cdr::AddDays(today, 15);
    EXPECT_NEAR(surface.Volatility(t_mid, 115.0), 0.21875, 1e-7);
}

TEST(VolatilityMathTest, DegenerateGridSinglePillar) {
    cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{today};
    cdr::DateType t1 = cdr::AddDays(today, 10);

    // Add only a single pillar for the entire date
    provider.AddPillar(t1, 100.0, 0.15).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_res = provider.ProvideSnapshot();
    const auto& surface = snapshot_res.Value();

    // Surface should be flat across all strikes for this date
    EXPECT_NEAR(surface.Volatility(t1, 50.0), 0.15, 1e-7);
    EXPECT_NEAR(surface.Volatility(t1, 100.0), 0.15, 1e-7);
    EXPECT_NEAR(surface.Volatility(t1, 200.0), 0.15, 1e-7);
}