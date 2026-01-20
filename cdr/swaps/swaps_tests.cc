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
        (day(10)/January/year(2021), cdr::Percent::FromFraction(0.20))
        .SetToday(today)
        .SetCalendar(&holiday_storage)
    ;

    cdr::IrsContract irs = cdr::IrsBuilder()
      .Coupon(cdr::Percent::FromFraction(0.24))
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
