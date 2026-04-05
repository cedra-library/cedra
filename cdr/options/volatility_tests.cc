#include <cdr/calendar/date.h>
#include <cdr/options/volatility.h>
#include <gtest/gtest.h>

#include <random>

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

TEST(VolatilityRCU, SnapshotSanity) {
    const cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider vsp{today};

    static const double kSpotPrice = 100.0;

    std::random_device seed_source{};

    std::default_random_engine strike_val_gen{seed_source()};
    std::default_random_engine volatility_val_gen{seed_source()};
    std::default_random_engine dates_num_gen{seed_source()};
    std::default_random_engine strikes_num_gen{seed_source()};

    std::uniform_real_distribution<> random_strike_value{0.5 * kSpotPrice, 1.5 * kSpotPrice};
    std::uniform_real_distribution<> random_volatility_value{0.05, 0.80};

    std::uniform_int_distribution<> random_dates_amount{10, 100};
    std::uniform_int_distribution<> random_strikes_amount{10, 100};

    {
        const auto provided = vsp.ProvideSnapshot();
        EXPECT_FALSE(provided) << "We didn't add pillars";
        ASSERT_EQ(provided.GetFailure(), cdr::Error::NoData) << "We didn't add pillars -> no data";
    }

    for (int iteration = 0; iteration < 100; iteration++) {
        std::vector<std::tuple<DateType, double, double>> pillars;
        pillars.reserve(100);

        cdr::DateType t1 = today;


        for (int days_offset = 0; days_offset < random_dates_amount(dates_num_gen); days_offset++) {
            t1 = cdr::AddDays(t1, days_offset);

            for (int j = 0; j < random_strikes_amount(strikes_num_gen); j++) {
                double pillar_strike = random_strike_value(strike_val_gen);
                double pillar_volatility = random_volatility_value(volatility_val_gen);
                pillars.emplace_back(t1, pillar_strike, pillar_volatility);

                cdr::Expect<void, cdr::Error> added = vsp.AddPillar(t1, pillar_strike, pillar_volatility);
                EXPECT_TRUE(added.Succeed()) << "Shouldn't fail";
            }
        }

        {
            cdr::Expect<void, cdr::Error> snapshot_updated = vsp.UpdateSnapshot();
            ASSERT_TRUE(snapshot_updated.Succeed()) << "Data inserted -> snapshot should be constructed correctly: "
                                                    << cdr::ErrorAsStringView(snapshot_updated);
        }

        cdr::Expect<cdr::VolatilitySurface, cdr::Error> snapshot_provided = vsp.ProvideSnapshot();
        ASSERT_TRUE(snapshot_provided) << "Update was successful, expected correct providing of snapshot"
                                       << cdr::ErrorAsStringView(snapshot_provided);

        for (const auto& [date, strike, expected_volatility] : pillars) {
            EXPECT_NEAR(snapshot_provided.Value().Volatility(date, strike), expected_volatility, 1e-7) << "Not match";
        }
    }
}


namespace {

constexpr double kEps = 1e-12;

}  // namespace

TEST(VolatilityRCU, ReferenceCountLifecycleSingleThread) {
    const cdr::DateType today = cdr::Today();
    cdr::VolatilitySurfaceProvider provider{today};

    const cdr::DateType t1 = cdr::AddDays(today, 10);
    provider.AddPillar(t1, 100.0, 0.10).OrCrashProgram();
    provider.AddPillar(t1, 110.0, 0.15).OrCrashProgram();
    provider.AddPillar(t1, 120.0, 0.20).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_res = provider.ProvideSnapshot();
    ASSERT_TRUE(snapshot_res.Succeed());
    cdr::VolatilitySurface surface = std::move(snapshot_res).Value();

    // Provider keeps one reference, the returned snapshot adds one more.
    EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), 2u);
    EXPECT_EQ(surface.Header().magic_number, cdr::VolatilitySurface::kMagicNumber);

    {
        cdr::VolatilitySurface copy = surface;
        EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), 3u);
        EXPECT_NEAR(copy.Volatility(t1, 110.0), 0.15, kEps);

        {
            cdr::VolatilitySurface moved = std::move(copy);
            EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), 3u);
            EXPECT_NEAR(moved.Volatility(t1, 100.0), 0.10, kEps);
        }

        EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), 2u);
    }

    EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), 2u);
}

TEST(VolatilityRCU, OldSnapshotSurvivesProviderUpdateAndDestruction) {
    const cdr::DateType today = cdr::Today();
    const cdr::DateType t1 = cdr::AddDays(today, 10);
    const cdr::DateType t2 = cdr::AddDays(today, 20);

    std::optional<cdr::VolatilitySurface> old_snapshot;

    {
        cdr::VolatilitySurfaceProvider provider{today};

        provider.AddPillar(t1, 100.0, 0.10).OrCrashProgram();
        provider.AddPillar(t1, 120.0, 0.20).OrCrashProgram();
        ASSERT_TRUE(provider.UpdateSnapshot());

        auto first_res = provider.ProvideSnapshot();
        ASSERT_TRUE(first_res.Succeed());
        old_snapshot.emplace(std::move(first_res).Value());

        EXPECT_NEAR(old_snapshot->Volatility(t1, 100.0), 0.10, kEps);
        EXPECT_NEAR(old_snapshot->Volatility(t1, 110.0), 0.15, kEps);

        // Rebuild the provider data and swap in a new snapshot.
        provider.AddPillar(t1, 100.0, 0.55).OrCrashProgram();
        provider.AddPillar(t1, 120.0, 0.75).OrCrashProgram();
        provider.AddPillar(t2, 100.0, 0.65).OrCrashProgram();
        provider.AddPillar(t2, 120.0, 0.85).OrCrashProgram();
        ASSERT_TRUE(provider.UpdateSnapshot());

        auto second_res = provider.ProvideSnapshot();
        ASSERT_TRUE(second_res.Succeed());
        cdr::VolatilitySurface new_snapshot = std::move(second_res).Value();

        EXPECT_NEAR(old_snapshot->Volatility(t1, 100.0), 0.10, kEps);
        EXPECT_NEAR(old_snapshot->Volatility(t1, 120.0), 0.20, kEps);

        EXPECT_NEAR(new_snapshot.Volatility(t1, 100.0), 0.55, kEps);
        EXPECT_NEAR(new_snapshot.Volatility(t1, 120.0), 0.75, kEps);
        EXPECT_NEAR(new_snapshot.Volatility(t2, 100.0), 0.65, kEps);
        EXPECT_NEAR(new_snapshot.Volatility(t2, 120.0), 0.85, kEps);
    }

    // Provider is already destroyed here; the old snapshot must still be valid.
    ASSERT_TRUE(old_snapshot.has_value());
    EXPECT_NEAR(old_snapshot->Volatility(t1, 100.0), 0.10, kEps);
    EXPECT_NEAR(old_snapshot->Volatility(t1, 110.0), 0.15, kEps);
}

TEST(VolatilityRCU, ConcurrentCopyAndDestructionLeavesRefcountBalanced) {
    const cdr::DateType today = cdr::Today();
    const cdr::DateType t1 = cdr::AddDays(today, 10);

    cdr::VolatilitySurfaceProvider provider{today};
    provider.AddPillar(t1, 100.0, 0.10).OrCrashProgram();
    provider.AddPillar(t1, 110.0, 0.15).OrCrashProgram();
    provider.AddPillar(t1, 120.0, 0.20).OrCrashProgram();

    ASSERT_TRUE(provider.UpdateSnapshot());
    auto snapshot_res = provider.ProvideSnapshot();
    ASSERT_TRUE(snapshot_res.Succeed());
    cdr::VolatilitySurface surface = std::move(snapshot_res).Value();

    const u32 refcount_before = surface.Header().reference_count.load(std::memory_order_acquire);
    ASSERT_EQ(refcount_before, 2u);

    constexpr int kThreads = 8;
    constexpr int kIterations = 5000;

    std::atomic<int> ready{0};
    std::atomic<bool> start{false};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < kIterations; ++i) {
                cdr::VolatilitySurface copy = surface;
                const double v1 = copy.Volatility(t1, 100.0);
                const double v2 = copy.Volatility(t1, 120.0);
                if (std::abs(v1 - 0.10) > kEps || std::abs(v2 - 0.20) > kEps) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }

                cdr::VolatilitySurface moved = std::move(copy);
                const double vm = moved.Volatility(t1, 110.0);
                if (std::abs(vm - 0.15) > kEps) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    while (ready.load(std::memory_order_acquire) < kThreads) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(failures.load(std::memory_order_acquire), 0);
    EXPECT_EQ(surface.Header().reference_count.load(std::memory_order_acquire), refcount_before);
}
