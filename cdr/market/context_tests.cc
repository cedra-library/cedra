#include <gtest/gtest.h>

#include <cdr/market/context.h>
#include <cdr/calendar/date.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/types/types.h>

using namespace std::chrono;

namespace {

TEST(MarketContext, Basic) {
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
    DateType today = day(31)/December/year(2024);

    cdr::MarketContext context(std::move(holiday_storage), today);
    cdr::MarketContextView view(context);
    ASSERT_EQ(context.Today(), today);
    ASSERT_EQ(view.Today(), today);

    context.SetToday(day(1)/January/year(2025));
    ASSERT_EQ(context.Today(), day(1)/January/year(2025));
    ASSERT_EQ(view.Today(), day(1)/January/year(2025));

    context.SetFxSpot({"USD", "RUB"}, 70.0);
    ASSERT_EQ(context.FxSpot({"USD", "RUB"}), 70.0);
    ASSERT_EQ(context.FxSpot({"RUB", "USD"}), 1.0 / 70.0);
    ASSERT_EQ(view.FxSpot({"USD", "RUB"}), 70.0);
    ASSERT_EQ(view.FxSpot({"RUB", "USD"}), 1.0 / 70.0);


    context.SetFxSpot({"USD", "RUB"}, 80.0);
    ASSERT_EQ(context.FxSpot({"USD", "RUB"}), 80.0);
    ASSERT_EQ(context.FxSpot({"RUB", "USD"}), 1.0 / 80.0);
    ASSERT_EQ(view.FxSpot({"USD", "RUB"}),80.0);
    ASSERT_EQ(view.FxSpot({"RUB", "USD"}), 1.0 / 80.0);
}

}
