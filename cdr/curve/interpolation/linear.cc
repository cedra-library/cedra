#include <cdr/curve/interpolation/linear.h>
#include <cdr/calendar/date.h>
#include <chrono>

namespace cdr {

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

}  // namespace cdr
