#include <cdr/curve/curve.h>
#include <cdr/curve/interpolation/linear.h>

namespace cdr {

Curve::CurveEasyInit Curve::StaticInit() {
    return CurveEasyInit{this};
}

void Curve::Clear() {
    points_.clear();
}

void Curve::Insert(DateType when, Percent value) {
    CDR_CHECK(!ctx_.Calendar().IsWeekend(jurisdiction_, when))
        << when << " must be buisness day for [" << jurisdiction_ << "]";
    points_[when] = value;
}

void Curve::ApplyFXContract(const Curve& other, const ForwardContract& fwd) {
    CDR_CHECK(fwd.GetPair().first == other.jurisdiction_ || fwd.GetPair().second == other.jurisdiction_)
        << "Forward contract could not be applied";
    CDR_CHECK(fwd.GetPair().first == jurisdiction_ || fwd.GetPair().second == jurisdiction_)
        << "Forward contract could not be applied";

    f64 price = fwd.GetPrice();
    if (fwd.GetPair().second == jurisdiction_) {
        price = 1. / price;
    }

    DateType settlement = ctx_.Calendar().AdvanceDateByConvention(jurisdiction_, fwd.GetTradeDate(), fwd.GetTenor());
    DateType spot_date = ctx_.SpotDate(fwd.GetPair());

    PointsContainer::iterator node;
    if (auto iter = points_.lower_bound(settlement);
        iter == points_.end() || iter->first != settlement) [[likely]] {
        node = points_.emplace_hint(iter, settlement, Percent::Zero());
    } else {
        node = iter;
    }

    Percent rate_d = other.Interpolated<Linear>(spot_date, ctx_.Calendar(), other.jurisdiction_);
    f64 spot_price = ctx_.FxSpot(fwd.GetPair());

    Percent rate_f = rate_d - Percent::FromFraction(std::log(spot_price / fwd.GetPrice()) / DayCountFraction({spot_date, settlement}));
    node->second = rate_f;
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
    return Percent::FromFraction(std::exp(-rate.Fraction() * DayCountFraction(period)));
}

/* static */
[[nodiscard]] Percent Curve::DiscountToZeroRates(const Period& period, Percent discount) {
    CDR_CHECK(discount.IsPositive()) << "discount factor must be positive";
    CDR_CHECK(period.Since() < period.Until()) << "period must be non-empty";
    return Percent::FromFraction(-std::log(discount.Fraction()) / DayCountFraction(period));
}

} // namespace cdr
