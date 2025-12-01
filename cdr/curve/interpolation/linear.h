#pragma once

#include <cdr/curve/curve.h>
#include <cdr/calendar/holiday_storage.h>

namespace cdr {

struct Linear {
public:
    static constexpr bool kStatefulImplementation = false;

    static Percent Interpolate(const Curve::PointsContainer& points,
                               const DateType& date,
                               const HolidayStorage& hs,
                               const std::string& jur);

};

}  // namespace cdr
