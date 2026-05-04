#include <algorithm>

#include <gtest/gtest.h>

#include <cdr/curve/curve.h>
#include <cdr/calendar/date.h>
#include <cdr/types/percent.h>
#include <cdr/curve/interpolation/linear.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/model/model.h>

using namespace std::chrono;
using cdr::Percent;
using cdr::Linear;

namespace {

struct TestInterpolation {
    static constexpr bool kStatefulImplementation = true;

    static Percent Interpolate(const cdr::Curve::PointsContainer& points,
                               const DateType& date,
                               const cdr::HolidayStorage& hs,
                               const JurisdictionType& jur)
    {
        return Percent::Zero();
    }

};

struct DummyContract {
    std::optional<f64> NPV(const cdr::Curve& curve) const {
        if (!rate.has_value()) {
            return std::nullopt;
        }
        return (target_rate - *rate).Apply(notional);
    }

    void ApplyCurve(const cdr::Curve& curve) {
        auto curve_rate = curve.Interpolated<Linear>(settlement_date, curve.Calendar(), jur);
        rate = curve_rate;
    }

    cdr::DateType SettlementDate() const {
        return settlement_date;
    }

    JurisdictionType jur;
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
    cdr::MarketContext context(std::move(hs), today);
    auto curve = cdr::CurveBuilder(context)
        .Jurisdiction("TEST")
        .Add(day(1)/January/year(2021), Percent::FromFraction(21))
        .FromPoints()
    ;

    auto query = day(1)/January/year(2001);
    auto incremented = [] (Percent p) { return p + Percent::FromFraction(1); };

    Percent pnt = curve->InterpolatedTransformed<cdr::Linear>(query, incremented, context.Calendar(), "TEST");
    ASSERT_EQ(pnt, Percent::FromFraction(22));

    TestInterpolation ti;
    Percent other =
        curve->InterpolatedTransformed<TestInterpolation>(query, incremented, ti,  context.Calendar(), "TEST");
    ASSERT_EQ(other, Percent::FromFraction(1));
}

TEST(Curve, RollForward) {
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("USD", day(3)/June/year(2027))
        ("USD", day(5)/June/year(2027))
        ("USD", day(7)/June/year(2027))
        ("USD", day(9)/June/year(2027))
        ("USD", day(11)/June/year(2027))
        ("WRONG", day(12)/June/year(2027))
        ("WRONG", day(13)/June/year(2027))
        ("WRONG", day(14)/June/year(2027))
    ;
    DateType today = day(1)/June/year(2027);

    cdr::MarketContext context(std::move(hs), today);
    auto curve = cdr::CurveBuilder(context)
        .Jurisdiction("USD")
        .Add(day(2)/June/year(2027), Percent::FromPercentage(1))
        .Add(day(4)/June/year(2027), Percent::FromPercentage(2))
        .Add(day(8)/June/year(2027), Percent::FromPercentage(3))
        .Add(day(10)/June/year(2027), Percent::FromPercentage(4))
        .FromPoints()
    ;
    std::set<DateType> target {
        day(4)/June/year(2027),
        day(8)/June/year(2027),
        day(10)/June/year(2027),
        day(14)/June/year(2027),
    };

    context.SetToday(context.Calendar().FindNextWorkingDay("USD", today));
    curve->RollForward();
    ASSERT_EQ(curve->Today(), day(2)/June/year(2027));

    const auto& pillars = curve->Pillars();

    ASSERT_EQ(pillars.size(), target.size());
    ASSERT_TRUE(std::equal(pillars.begin(), pillars.end(), target.begin(),
                            [](const auto& point, const auto& date) {
                                return point.first == date;
                            }));
}

TEST(Curve, DummyContract) {
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("TEST", day(31)/December/year(2025))
        ("TEST", day(2)/January/year(2026))
    ;
    DateType today = day(1)/January/year(2026);
    DummyContract contract {
        .jur = "TEST",
        .settlement_date = day(3)/January/year(2026),
        .rate = std::nullopt,
        .target_rate = cdr::Percent::FromPercentage(20.),
        .notional = 100.,
    };

    cdr::MarketContext context(std::move(hs), today);

    auto curve = cdr::CurveBuilder(context)
        .Jurisdiction("TEST")
        .FromPoints()
    ;

    curve->AdaptToContract(contract);
    ASSERT_TRUE(curve->Pillars().size() == 1);
    ASSERT_TRUE(curve->Pillars().contains(contract.SettlementDate()) == 1);
    ASSERT_NEAR(contract.rate.value().Fraction(), contract.target_rate.Fraction(), 0.001);
}

TEST(Curve, CurveFromContracts) {
    cdr::HolidayStorage hs;
    hs.StaticInit()
        ("TEST", day(31)/December/year(2025))
        ("TEST", day(2)/January/year(2026))
    ;
    DateType today = day(1)/January/year(2026);
    DummyContract contract {
        .jur = "TEST",
        .settlement_date = day(3)/January/year(2026),
        .rate = Percent::FromPercentage(20.),
        .target_rate = Percent::FromPercentage(20.),
        .notional = 100.,
    };

    cdr::MarketContext context(std::move(hs), today);

    auto curve = cdr::CurveBuilder(context)
        .Jurisdiction("TEST")
        .FromContracts(&contract, &contract + 1)
    ;

    ASSERT_TRUE(curve->Pillars().size() == 1);
    ASSERT_TRUE(curve->Pillars().contains(contract.SettlementDate()) == 1);

    contract.rate = std::nullopt;
    contract.ApplyCurve(*curve);
    ASSERT_NEAR(contract.rate.value().Fraction(), contract.target_rate.Fraction(), 0.001);
}
