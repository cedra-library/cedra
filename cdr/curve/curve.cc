#include <cdr/curve/curve.h>

namespace cdr {

Curve::CurveEasyInit Curve::StaticInit() {
    return CurveEasyInit{this};
}

void Curve::Clear() {
    points_.clear();
}

void Curve::Insert(DateType when, Percent value) {
    CDR_CHECK(!calendar_->IsWeekend(jurisdiction_, when))
        << when << " must be buisness day for [" << jurisdiction_ << "]";
    points_[when] = value;
}

void Curve::RollForward() noexcept {
    CDR_CHECK(calendar_ != nullptr) << "calendar should be defined";
    // moving backwards to avoid key collisions
    for (auto it = points_.end(); it != points_.begin();) {
        auto hint = it--;
        auto node = points_.extract(it);
        node.key() = calendar_->FindNextWorkingDay(jurisdiction_, node.key());
        it = points_.insert(hint, std::move(node));
    }
    today_ = calendar_->FindNextWorkingDay(jurisdiction_, today_);
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
