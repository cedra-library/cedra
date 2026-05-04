#include <cdr/curve/curve.h>
#include <cdr/curve/interpolation/linear.h>

namespace cdr {

/* static */
[[nodiscard]] std::unique_ptr<Curve> Curve::Create(MarketContextView ctx, const JurisdictionType& jur) {
    return std::unique_ptr<Curve>(new Curve(ctx, jur));
}

void Curve::Clear() {
    points_.clear();
}

void Curve::Insert(DateType when, Percent value) {
    CDR_CHECK(!ctx_.Calendar().IsWeekend(jurisdiction_, when))
        << when << " must be buisness day for [" << jurisdiction_ << "]";
    points_[when] = value;
}

void Curve::ApplyFXContract(const Curve& other, const ForwardContract& fwd) noexcept {
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
    // moving backwards to avoid key collisions
    for (auto it = points_.end(); it != points_.begin();) {
        auto hint = it--;
        auto node = points_.extract(it);
        node.key() = Calendar().FindNextWorkingDay(jurisdiction_, node.key());
        it = points_.insert(hint, std::move(node));
    }
}

/* static */
[[nodiscard]] Percent Curve::ZeroRatesToDiscount(const DateType& date, Percent rate) const {
    return Percent::FromFraction(std::exp(-rate.Fraction() * DayCountFraction(Period{Today(), date})));
}

/* static */
[[nodiscard]] Percent Curve::DiscountToZeroRates(const DateType& date, Percent discount) const {
    CDR_CHECK(discount.IsPositive()) << "discount factor must be positive";
    CDR_CHECK(Today() < date) << "period must be non-empty";
    return Percent::FromFraction(-std::log(discount.Fraction()) / DayCountFraction(Period{Today(), date}));
}

CurveBuilder& CurveBuilder::Add(const DateType& when, Percent value) {
    points_.emplace(when, value);
    return *this;
}

[[nodiscard]] std::unique_ptr<Curve> CurveBuilder::FromPoints() {
    CDR_CHECK(jurisdiction_.has_value()) << "Jusrisdiction should be set";

    auto curve = Curve::Create(ctx_, std::move(*jurisdiction_));
    curve->points_ = std::move(points_);

    return curve;
}

} // namespace cdr
