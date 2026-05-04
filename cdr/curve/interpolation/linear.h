#pragma once

#include <cdr/curve/curve.h>
#include <cdr/calendar/holiday_storage.h>

namespace cdr {

struct CDR_CURVE_EXPORT Linear {
public:
    static constexpr bool kStatefulImplementation = false;

    static Percent Interpolate(const Curve::PointsContainer& points,
                               const DateType& date,
                               const HolidayStorage& hs,
                               const JurisdictionType& jur);

    // Deprecated. Use cdr/math instead.
    static f64 InterpolateDerivative(const Curve::PointsContainer& points,
                                     const DateType& date,
                                     const HolidayStorage& hs,
                                     const JurisdictionType& jur);

    [[deprecated("use cdr/math instead")]]
    static f64 YetAnotherDerivative(const Curve::PointsContainer& points,
                                     const DateType& date,
                                     const DateType& pillar,
                                     const HolidayStorage& hs,
                                     const JurisdictionType& jur);

};

}  // namespace cdr
