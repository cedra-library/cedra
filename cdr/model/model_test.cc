#include <gtest/gtest.h>
#include <cdr/model/model.h>
#include <cdr/types/percent.h>
#include <cdr/calendar/date.h>
#include <cdr/curve/curve.h>
#include <cdr/market/context.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/swaps/irs.h>

#include <chrono>

TEST(Model, Basic) {
    using namespace std::chrono;
    using namespace cdr::literals;

    cdr::HolidayStorage holiday_storage;

    holiday_storage.StaticInit()
        ("USD", year(2025) / January / day(1))
        ("USD", year(2025) / January / day(2))
        ("USD", year(2025) / January / day(3))
        ("USD", year(2025) / January / day(4))
        ("USD", year(2025) / January / day(5))
        ("USD", year(2025) / January / day(6))
        ("USD", year(2025) / January / day(7))
        ("USD", year(2025) / January / day(8))
        ("USD", year(2025) / January / day(9))
    ;
    cdr::DateType today = day(31)/December/year(2024);

    cdr::MarketContext context(std::move(holiday_storage), today);
    cdr::Model model(context);

    ASSERT_TRUE(model.BuildMainCurve("USD").Succeed());
    auto *curve = model.GetCurve("USD");
    ASSERT_NE(curve, nullptr);
    ASSERT_TRUE(curve->Pillars().empty());
}
