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

} // anonymous namespace

TEST(Curve, BasicOps) {
    cdr::Curve curve;

    curve.StaticInit()
      (day(1)/January/year(2021), Percent::FromFraction(21))
      (day(2)/January/year(2021), Percent::FromFraction(22))
      (day(3)/January/year(2021), Percent::FromFraction(23))
    ;
    auto query = day(1)/January/year(2001);
    auto incremented = [] (Percent p) { return p + Percent::FromFraction(1); };

    cdr::HolidayStorage hs;
    hs.StaticInit()
      ("TEST", day(1)/February/year(2001))
    ;

    Percent pnt = curve.InterpolatedTransformed<cdr::Linear>(query, incremented, hs, "TEST");
    ASSERT_EQ(pnt, Percent::FromFraction(22));

    TestInterpolation ti;
    Percent other = curve.InterpolatedTransformed<TestInterpolation>(query, incremented, ti,  hs, "TEST");
    ASSERT_EQ(other, Percent::FromFraction(1));
}

