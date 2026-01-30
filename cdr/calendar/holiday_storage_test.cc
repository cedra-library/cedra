#include <gtest/gtest.h>
#include <cdr/calendar/holiday_storage.h>

TEST(HStorage, basic) {
    using namespace std::chrono;

    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("RUS", year(2025)/January/day(1))
        ("RUS", year(2025)/January/day(2))
        ("RUS", year(2025)/January/day(3))
        ("RUS", year(2025)/January/day(4))
        ("RUS", year(2025)/January/day(5))
        ("RUS", year(2025)/January/day(6))
        ("RUS", year(2025)/January/day(7))
        ("RUS", year(2025)/January/day(8))
        ("RUS", year(2025)/January/day(9))
    ;

    DateType sett = day(31)/December/2024;
    DateType matur = day(14)/January/2025;
    cdr::Period per(sett, matur);


    auto buisness_days = hs.BusinessDays(per.WithFrequency(cdr::Freq::kDaily), "RUS");

    for (const auto& day : buisness_days) {
        std::cerr << day << std::endl;
    }
}
