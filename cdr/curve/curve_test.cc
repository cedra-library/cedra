#include <gtest/gtest.h>

#include <cdr/curve/curve.h>
#include <cdr/calendar/date.h>
#include <cdr/types/percent.h>
#include <cdr/curve/interpolation/linear.h>
#include <cdr/calendar/holiday_storage.h>

using namespace std::chrono;
using cdr::Percent;
using cdr::Linear;

namespace {

struct TestInterpolation {
    static constexpr bool kStatefulImplementation = true;

    static Percent Interpolate(const cdr::Curve::PointsContainer& points,
                               const DateType& date,
                               const cdr::HolidayStorage& hs,
                               const std::string& jur)
    {
        return Percent::Zero();
    }

};

struct DummyContract {
    std::optional<f64> NPV(cdr::Curve *curve) const {
        if (!rate.has_value()) {
            return std::nullopt;
        }
        return (target_rate - *rate).Apply(notional);
    }

    void ApplyCurve(cdr::Curve* curve) {
        auto curve_rate = curve->Interpolated<Linear>(settlement_date, *curve->Calendar(), jur);
        rate = curve_rate;
    }

    cdr::DateType SettlementDate() const {
        return settlement_date;
    }

    std::string jur;
    cdr::DateType settlement_date;
    std::optional<cdr::Percent> rate;
    cdr::Percent target_rate;
    f64 notional;
};
static_assert(cdr::Contract<DummyContract>);

} // anonymous namespace

TEST(Curve, BasicOps) {
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("TEST", day(1)/February/year(2001))
    ;
    cdr::DateType today = day(31)/December/year(2020);

    cdr::Curve curve;
    curve.StaticInit()
        (day(1)/January/year(2021), Percent::FromFraction(21))
        (day(2)/January/year(2021), Percent::FromFraction(22))
        (day(3)/January/year(2021), Percent::FromFraction(23))
        .SetToday(today)
        .SetCalendar(&hs)
    ;

    auto query = day(1)/January/year(2001);
    auto incremented = [] (Percent p) { return p + Percent::FromFraction(1); };

    Percent pnt = curve.InterpolatedTransformed<cdr::Linear>(query, incremented, hs, "TEST");
    ASSERT_EQ(pnt, Percent::FromFraction(22));

    TestInterpolation ti;
    Percent other = curve.InterpolatedTransformed<TestInterpolation>(query, incremented, ti,  hs, "TEST");
    ASSERT_EQ(other, Percent::FromFraction(1));
}

TEST(Curve, DummyContract) {
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("TEST", day(31)/December/year(2025))
        ("TEST", day(2)/January/year(2026))
    ;

    cdr::DateType today = day(1)/January/year(2026);

    cdr::Curve curve;

    curve.StaticInit()
         .SetToday(today)
         .SetCalendar(&hs)
    ;

    DummyContract contract {"TEST", day(3)/January/year(2026), std::nullopt, cdr::Percent::FromFraction(0.20), 100.};
    std::cerr << "before adapt\n";
    curve.AdaptToContract(&contract);

    std::cerr << "contract: " << contract.rate.value().Fraction() << " vs " << contract.target_rate.Fraction() << '\n';


    // curve.AdaptToContract

}
