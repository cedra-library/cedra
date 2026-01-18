#include <cdr/curve/curve.h>

namespace cdr {

Curve::CurveEasyInit Curve::StaticInit() {
    return CurveEasyInit{this};
}

void Curve::Clear() {
    points_.clear();
}

void Curve::Insert(DateType when, Percent value) {
    points_[when] = value;
}

/* static */
[[nodiscard]] Percent Curve::ZeroRatesToDiscount(const Period& period, Percent rate) {
    return Percent::FromFraction(1. / (1. + rate.Fraction() * DayCountFraction(period)));
}

/* static */
[[nodiscard]] Percent Curve::DiscountToZeroRates(const Period& period, Percent discount) {
    CDR_CHECK(discount.IsPositive()) << "discount factor must be positive";
    CDR_CHECK(period.Since() < period.Until()) << "period must be non-empty";
    return Percent::FromFraction(((1. / discount.Fraction()) - 1) / DayCountFraction(period));
}

} // namespace cdr
