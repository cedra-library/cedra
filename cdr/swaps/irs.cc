#include <cdr/swaps/irs.h>

#include <queue>
#include <utility>
#include <cdr/base/check.h>
#include <cdr/calendar/date.h>
#include <cdr/curve/interpolation/linear.h>

namespace cdr {

[[nodiscard]] std::optional<f64> IrsContract::NPV(Curve *curve) const noexcept {
    auto pv_fixed = PVFixed(curve);
    auto pv_float = PVFloat(curve);
    if (!pv_fixed.has_value() || !pv_float.has_value()) {
        return std::nullopt;
    }
    f64 res = *pv_fixed - *pv_float;
    if (PayFix()) [[likely]] {
        return res;
    }
    return -res;
}

[[nodiscard]] std::optional<f64> IrsContract::PVFixed(Curve *curve) const noexcept {
    auto fixed_leg = FixedLeg();
    Period period = {curve->Today(), {}};
    auto [today, settlement_date] = period.TupleLike();

    f64 result = 0.;

    for (const auto& payment_period : fixed_leg) {
        if (payment_period.Until() < today) {
            continue;
        }
        settlement_date = payment_period.SettlementDate();
        auto rate = curve->Interpolated<Linear>(settlement_date, *curve->Calendar(), jurisdiction_);
        result += DayCountFraction(period) * Curve::ZeroRatesToDiscount(period, rate).Fraction();
    }

    return result * coupon_.Fraction() * notional_;
}

[[nodiscard]] std::optional<f64> IrsContract::PVFloat(Curve *curve) const noexcept {
    auto float_leg = FloatLeg();
    Period period = {curve->Today(), {}};
    auto [today, settlement_date] = period.TupleLike();

    auto begin = std::lower_bound(float_leg.begin(), float_leg.end(), today,
                                 [](const IrsPaymentPeriod& period, const DateType& today) {
                                    return period.Until() < today;
                                 });
    f64 result = 0.;

    for (const auto& payment_period : float_leg) {
        if (payment_period.Until() < today) {
            continue;
        }
        settlement_date = payment_period.SettlementDate();
        if (!payment_period.HasKnownPayment()) {
            return std::nullopt;
        }
        auto rate = curve->Interpolated<Linear>(settlement_date, *curve->Calendar(), jurisdiction_);
        result += *payment_period.Payment() * DayCountFraction(period) * Curve::ZeroRatesToDiscount(period, rate).Fraction();
    }

    return result;
}

void IrsContract::ApplyCurve(Curve* curve) noexcept {
    auto leg = FloatLegMut();

    for (auto& period : leg) {
        auto rate = curve->Interpolated<Linear>(period.Until(), *curve->Calendar(), jurisdiction_);
        f64 payment = (rate + adjustment_).Apply(notional_);
        period.SetPayment(payment);
    }
}

/* IrsBuilder */

[[nodiscard]] IrsContract IrsBuilder::Build(const HolidayStorage& hs, const std::string& jur, DateRollingRule rule) {

    CDR_CHECK(jurisdiction_.has_value()) << "must be defined";
    CDR_CHECK(maturity_date_.has_value()) << "must be defined";
    CDR_CHECK(settlement_date_.has_value()) << "must be defined";
    CDR_CHECK(cpn_.has_value()) << "must be defined";
    CDR_CHECK(adjustment_.has_value()) << "must be defined";
    CDR_CHECK(notional_.has_value()) << "must be defined";
    CDR_CHECK(paying_fix_.has_value()) << "must be defined";
    CDR_CHECK(fixed_freq_.has_value()) << "must be defined";
    CDR_CHECK(float_freq_.has_value()) << "must be defined";

    static constexpr u32 kRandomReservationConstant = 10;
    IrsContract result(cpn_.value(), paying_fix_.value());

    std::vector<IrsPaymentPeriod> sched;
    sched.reserve(kRandomReservationConstant);
    auto period = Period(settlement_date_.value(), maturity_date_.value());
    auto compare_dates = [&sched](const u32 left_idx, const u32 right_idx) {
        return sched[left_idx].Since() > sched[right_idx].Since();
    };
    std::priority_queue<u32, std::vector<u32>, decltype(compare_dates)> chronological_order(compare_dates);

    f64 fixed_payment = cpn_->Apply(notional_.value());
    u32 idx = 0;

    std::optional<DateType> since = std::nullopt;
    std::optional<DateType> until = std::nullopt;

    for (DateType date : hs.BuisnessDays(period.WithFrequency(fixed_freq_.value()), jur, rule)) {
        since = std::exchange(until, date);
        if (!since) [[unlikely]] {
            continue;
        }

        auto payment_period = Period{since.value(), std::min(until.value(), maturity_date_.value())};
        sched.emplace_back(payment_period, fixed_payment * DayCountFraction(payment_period));
        chronological_order.push(idx++);

        if (payment_period.Until() == maturity_date_) {
            break;
        }
    }

    since = std::nullopt;
    until = std::nullopt;
    u64 fixed_last = sched.size();

    for (DateType date : hs.BuisnessDays(period.WithFrequency(float_freq_.value()), jur, rule)) {
        since = std::exchange(until, date);
        if (!since) [[unlikely]] {
            continue;
        }

        auto payment_period = Period{since.value(), std::min(until.value(), maturity_date_.value())};
        sched.emplace_back(payment_period);
        chronological_order.push(idx++);

        if (payment_period.Until() == maturity_date_) {
            break;
        }
    }

    u32 last = IrsPaymentPeriod::kNotInitialized;
    u32 curr = IrsPaymentPeriod::kNotInitialized;
    result.chrono_start_idx_ = chronological_order.top();

    while (!chronological_order.empty()) {
        last = std::exchange(curr, chronological_order.top());
        chronological_order.pop();

        sched[curr].chrono_prev_idx_ = last;

        if (last == IrsPaymentPeriod::kNotInitialized) [[unlikely]] {
            continue;
        }
        sched[last].chrono_next_idx_ = curr;
    }

    result.jurisdiction_ = std::move(*jurisdiction_);
    result.chrono_last_idx_ = last;
    result.notional_ = *notional_;
    result.payment_periods_ = std::move(sched);
    result.fixed_leg_ = result.payment_periods_.data();
    result.float_leg_ = result.payment_periods_.data() + fixed_last;

    Reset();
    return result;
}

void IrsBuilder::Reset() {
    jurisdiction_ = std::nullopt;
    maturity_date_ = std::nullopt;
    settlement_date_ = std::nullopt;
    effective_date_ = std::nullopt;
    fixed_freq_ = std::nullopt;
    float_freq_ = std::nullopt;
    cpn_ = std::nullopt;
    adjustment_ = std::nullopt;
    notional_ = std::nullopt;
    paying_fix_ = std::nullopt;
}

} // namespace cdr
