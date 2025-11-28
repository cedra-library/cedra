#include <gtest/gtest.h>
#include <cdr/swaps/irs.h>
#include <cdr/types/percent.h>

#include <chrono>

#include <cdr/calendar/date.h>
#include <cdr/curve/curve.h>

TEST(Basic, Option) {
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

    cdr::IrsContract irs = cdr::IrsBuilder()
                              .Coupon(cdr::Percent::FromPercentage(0.24))
                              .PayFix(true)
                              .Notion(2'000'000)
                              .FixedFreq(cdr::Freq::kQuarterly)
                              .FloatFreq(cdr::Freq::kAnnualy)
                              .MaturityDate(day(1) / January / year(2025))
                              .EffectiveDate(day(1) / January / year(2023))
                              .Build(holiday_storage, "RUS");

    for (const cdr::PaymentPeriodEntry& payment : irs.FixedLeg()) {
        ASSERT_TRUE(payment.HasKnownPayment());
    }

    for (const cdr::PaymentPeriodEntry& payment : irs.FloatLeg()) {
        ASSERT_FALSE(payment.HasKnownPayment());
    }
}
