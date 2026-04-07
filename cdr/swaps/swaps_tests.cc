#include <gtest/gtest.h>
#include <cdr/swaps/irs.h>
#include <cdr/types/percent.h>

#include <chrono>

#include <cdr/calendar/date.h>
#include <cdr/curve/curve.h>

TEST(Swaps, Basic) {
    using namespace std::chrono;
    using namespace cdr::literals;

    cdr::HolidayStorage holiday_storage;

    holiday_storage.StaticInit()
        ("RUS", year(2025) / January / day(1))
        ("RUS", year(2025) / January / day(2))
        ("RUS", year(2025) / January / day(3))
        ("RUS", year(2025) / January / day(4))
        ("RUS", year(2025) / January / day(5))
        ("RUS", year(2025) / January / day(6))
        ("RUS", year(2025) / January / day(7))
        ("RUS", year(2025) / January / day(8))
        ("RUS", year(2025) / January / day(9))
    ;
    cdr::DateType today = day(31)/December/year(2024);

    cdr::Curve curve;
    curve.StaticInit()
        .SetToday(today)
        .SetCalendar(&holiday_storage)
        .SetJurisdiction("RUS")
    ;

    cdr::IrsContract irs = cdr::IrsBuilder()
      .FixedRate(cdr::Percent::FromFraction(0.24))
      .PayFix(true)
      .Notion(2'000'000)
      .FixedFreq(cdr::Freq::kQuarterly)
      .FloatFreq(cdr::Freq::kAnnualy)
      .MaturityDate(day(1) / January / year(2025))
      .SettlementDate(day(1) / January / year(2023))
      .Adjustment(cdr::Percent::Zero())
      .Build(holiday_storage, "RUS")
    ;

    for (const cdr::IrsPaymentPeriod& payment : irs.FixedLeg()) {
        ASSERT_TRUE(payment.HasKnownPayment());
    }

    for (const cdr::IrsPaymentPeriod& payment : irs.FloatLeg()) {
        ASSERT_FALSE(payment.HasKnownPayment());
    }

    auto pv_fixed = irs.PVFixed(&curve);
    ASSERT_TRUE(pv_fixed.has_value());
}

TEST(Basic, Experimental) {
    using namespace std::chrono;
    using namespace cdr::literals;

    cdr::HolidayStorage holiday_storage;
    holiday_storage.StaticInit()
        ("RUB", year(2025) / December / day(31))
        ("RUB", year(2026) / January / day(1))
        ("RUB", year(2026) / January / day(2))
        ("RUB", year(2026) / January / day(3))
        ("RUB", year(2026) / January / day(4))
        ("RUB", year(2026) / January / day(5))
        ("RUB", year(2026) / January / day(6))
        ("RUB", year(2026) / January / day(7))
        ("RUB", year(2026) / January / day(8))
        ("RUB", year(2026) / January / day(9))
    ;
    DateType today = day(29)/December/year(2025);
    DateType tomorrow = day(30)/December/year(2025);
    DateType jan12 = day(12)/January/year(2026);

    cdr::Curve curve;
    curve.StaticInit()
        .SetToday(today)
        .SetJurisdiction("RUB")
        .SetCalendar(&holiday_storage)
    ;
    cdr::IrsContract swap = cdr::IrsBuilderExperimental()
        .Adjustment(0_percents)
        .FixedFreq({1, cdr::TimeUnit::Week})
        .FloatFreq({1, cdr::TimeUnit::Week})
        .FixedTerm({1, cdr::TimeUnit::Week})
        .FloatTerm({1, cdr::TimeUnit::Week})
        .Notion(1000)
        .PayFix(true)
        .PaymentDateShift(0)
        .StartShift(1)
        .Stub(cdr::IrsContract::Stub::SHORT)
        .TradeDate(today)
        .Build(holiday_storage, "RUB", cdr::DateRollingRule::kModifiedFollowing)
    ;

    ASSERT_TRUE(swap.FixedLeg().size() == 1);
    ASSERT_EQ(swap.FixedLeg().back().Since(), tomorrow);
    ASSERT_EQ(swap.FixedLeg().back().Until(), jan12);
}
