#include <cdr/curve/interpolation/linear.h>
#include <cdr/calendar/date.h>
#include <chrono>
#include <numeric>

namespace cdr {

// static
cdr::Percent Linear::Interpolate(const cdr::Curve::PointsContainer& points, const DateType& date,
                                const HolidayStorage& hs, const std::string& jur)
{
    if (hs.IsWeekend(jur, date)) {
        return Interpolate(points, hs.FindPreviousWorkingDay(jur, date), hs, jur);
    }

    if (points.empty()) [[unlikely]] {
        return Percent::Zero();
    }

    auto lo_it = points.lower_bound(date);

    if (lo_it == points.end()) [[unlikely]] {
        return std::prev(lo_it)->second;
    }

    if (lo_it->first == date) {
        return lo_it->second;
    }

    if (lo_it == points.begin()) [[unlikely]] {
        return lo_it->second;
    }

    auto up_it = points.upper_bound(date);

    const auto& [lo_date, lo_value] = *lo_it;
    const auto& [up_date, up_value] = *up_it;

    auto lo_time = std::chrono::sys_days(lo_date).time_since_epoch().count();
    auto up_time = std::chrono::sys_days(up_date).time_since_epoch().count();
    auto mid_time = std::chrono::sys_days(date).time_since_epoch().count();

    f64 factor = f64(mid_time - lo_time) / f64(up_time - lo_time);
    return lo_value + (up_value - lo_value) * factor;
}

// static
f64 Linear::InterpolateDerivative(const cdr::Curve::PointsContainer& points, const DateType& date,
                                  const HolidayStorage& hs, const std::string& jur)
{
    if (hs.IsWeekend(jur, date)) {
        return InterpolateDerivative(points, hs.FindPreviousWorkingDay(jur, date), hs, jur);
    }

    if (points.size() <= 1) [[unlikely]] {
        return 0.;
    }

    auto it = points.lower_bound(date);
    if (it == points.end()) {
        return 0.;
    }
    if (it->first == date) {
        auto mid = it->second;

        Percent left_value;
        u32 left_dist;
        if (it == points.begin()) {
            left_value = mid;
            left_dist = 1;
        } else {
            auto left_it = std::prev(it);
            left_value = it->second;
            left_dist = hs.CountBuisnessDays(left_it->first, date, jur);
            CDR_CHECK(left_dist > 0);
        }

        Percent right_value;
        u32 right_dist;
        if (it == std::prev(points.end())) {
            right_value = mid;
            right_dist = 1;
        } else {
            auto right_it = std::next(it);
            right_value = right_it->second;
            right_dist = hs.CountBuisnessDays(right_it->first, date, jur);
            CDR_CHECK(right_dist > 0);
        }

        auto left_coef = (it->second - left_value).Fraction() / left_dist;
        auto right_coef = (it->second - right_value).Fraction() / right_dist;
        return std::midpoint(left_coef, right_coef);
    }
    if (it == points.begin()) {
        return 0.;
    }
    auto left_it = std::prev(it);
    auto dist = hs.CountBuisnessDays(left_it->first, it->first, jur);
    return (it->second - left_it->second).Fraction() / dist;
}

// static
[[deprecated("use cdr/math instead")]]
f64 Linear::YetAnotherDerivative(const Curve::PointsContainer& points, const DateType& date,
                                 const DateType& pillar_date, const HolidayStorage& hs, const std::string& jur)
{
    CDR_CHECK(points.find(pillar_date) != points.end());
    if (pillar_date == date) {
        return 1.;
    }
    auto pillar_it = points.find(pillar_date);
    if (pillar_it == points.begin()) {
        if (date < pillar_date) {
            return 1.;
        }

    }
    CDR_CHECK(false) << "Not implemented";
    return 0.;
}

}  // namespace cdr
