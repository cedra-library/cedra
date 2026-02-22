#include <cdr/swaps/irs.h>

#include <queue>
#include <utility>
#include <cdr/base/check.h>
#include <cdr/calendar/date.h>
#include <cdr/curve/interpolation/linear.h>

namespace cdr {

[[nodiscard]] std::optional<f64> IrsContract::dNPV(Curve *curve, const DateType& date, Percent rate) const noexcept {
    auto dpv_fixed = dPVFixed(curve, date, rate);
    auto dpv_float = dPVFloat(curve, date, rate);
    if (!dpv_fixed.has_value() || !dpv_float.has_value()) [[unlikely]] {
        return std::nullopt;
    }
    std::optional<f64> res = std::make_optional<f64>(*dpv_float - *dpv_fixed);
    if (!PayFix()) [[unlikely]] {
        *res *= -1;
    }
    return res;
}

[[nodiscard]] std::optional<f64> IrsContract::dPVFixed(Curve *curve, const DateType& date, Percent rate) const noexcept {
    const auto& pillars = curve->Pillars();
    auto node = pillars.find(date);
    CDR_CHECK(node != pillars.end()) << "date should be present";
    CDR_CHECK(node->second == rate) << "rates should be equal";
    auto fixed_leg = FixedLeg();
    decltype(fixed_leg)::iterator begin;
    if (node == pillars.begin()) {
        begin = fixed_leg.begin();
    } else {
        auto node_prev = std::prev(node);
        begin = std::upper_bound(fixed_leg.begin(), fixed_leg.end(), node_prev->first, [](const DateType& val, const IrsPaymentPeriod& period) {
                return period.SettlementDate() < val;
            });
    }
    if (begin == fixed_leg.end()) [[unlikely]] {
        return 0.;
    }
    decltype(begin) end;


    return {};
}

[[nodiscard]] std::optional<f64> IrsContract::dPVFloat(Curve *curve, const DateType& date, Percent rate) const noexcept {
    const auto& pillars = curve->Pillars();
    return {};
}

[[nodiscard]] std::optional<f64> IrsContract::NPV(Curve *curve) const noexcept {
    auto pv_fixed = PVFixed(curve);
    auto pv_float = PVFloat(curve);
    if (!pv_fixed.has_value() || !pv_float.has_value()) [[unlikely]] {
        return std::nullopt;
    }
    std::optional<f64> res = std::make_optional<f64>(*pv_float - *pv_fixed);
    if (!PayFix()) [[unlikely]] {
        *res *= -1;
    }
    return res;
}

[[nodiscard]] std::optional<f64> IrsContract::PVFixed(Curve *curve) const noexcept {
    auto fixed_leg = FixedLeg();
    Period period = {curve->Today(), curve->Today()};
    auto& [today, settlement_date] = period;

    f64 result = 0.;

    for (const auto& payment_period : fixed_leg) {
        if (payment_period.Until() < today) {
            continue;
        }
        settlement_date = payment_period.SettlementDate();
        auto rate = curve->Interpolated<Linear>(settlement_date, *curve->Calendar(), jurisdiction_);
        result += DayCountFraction(period) * Curve::ZeroRatesToDiscount(period, rate).Fraction();
    }

    return result * fixed_rate_.Fraction() * notional_;
}

[[nodiscard]] std::optional<f64> IrsContract::PVFloat(Curve *curve) const noexcept {
    auto float_leg = FloatLeg();
    Period period = {curve->Today(), curve->Today()};
    auto& [today, settlement_date] = period;

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

    CDR_CHECK(maturity_date_.has_value()) << "must be defined";
    CDR_CHECK(settlement_date_.has_value()) << "must be defined";
    CDR_CHECK(fixed_rate_.has_value()) << "must be defined";
    CDR_CHECK(adjustment_.has_value()) << "must be defined";
    CDR_CHECK(notional_.has_value()) << "must be defined";
    CDR_CHECK(paying_fix_.has_value()) << "must be defined";
    CDR_CHECK(fixed_freq_.has_value()) << "must be defined";
    CDR_CHECK(float_freq_.has_value()) << "must be defined";
    CDR_CHECK(!jur.empty()) << "must be non-empty";

    static constexpr u32 kRandomReservationConstant = 10;
    IrsContract result(fixed_rate_.value(), paying_fix_.value());

    std::vector<IrsPaymentPeriod> sched;
    sched.reserve(kRandomReservationConstant);
    auto period = Period(settlement_date_.value(), maturity_date_.value());
    auto compare_dates = [&sched](const u32 left_idx, const u32 right_idx) {
        return sched[left_idx].Since() > sched[right_idx].Since();
    };
    std::priority_queue<u32, std::vector<u32>, decltype(compare_dates)> chronological_order(compare_dates);

    f64 fixed_payment = fixed_rate_->Apply(notional_.value());
    u32 idx = 0;

    std::optional<DateType> since = std::nullopt;
    std::optional<DateType> until = std::nullopt;

    for (DateType date : hs.BusinessDays(period.WithFrequency(fixed_freq_.value()), jur, rule)) {
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

    for (DateType date : hs.BusinessDays(period.WithFrequency(float_freq_.value()), jur, rule)) {
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

    result.jurisdiction_ = jur;
    result.chrono_last_idx_ = last;
    result.notional_ = *notional_;
    result.payment_periods_ = std::move(sched);
    result.fixed_leg_ = result.payment_periods_.data();
    result.float_leg_ = result.payment_periods_.data() + fixed_last;

    Reset();
    return result;
}

void IrsBuilder::Reset() {
    maturity_date_ = std::nullopt;
    settlement_date_ = std::nullopt;
    effective_date_ = std::nullopt;
    fixed_freq_ = std::nullopt;
    float_freq_ = std::nullopt;
    fixed_rate_ = std::nullopt;
    adjustment_ = std::nullopt;
    notional_ = std::nullopt;
    paying_fix_ = std::nullopt;
}

/* IrsBuilderExperimental */

[[nodiscard]] IrsContract IrsBuilderExperimental::Build(const HolidayStorage& hs, const std::string& jur, DateRollingRule rule) {

    CDR_CHECK(trade_date_.has_value()) << "must be defined";
    CDR_CHECK(start_shift_.has_value()) << "must be defined";
    CDR_CHECK(fixed_term_.has_value()) << "must be defined";
    CDR_CHECK(float_term_.has_value()) << "must be defined";
    CDR_CHECK(fixed_freq_.has_value()) << "must be defined";
    CDR_CHECK(float_freq_.has_value()) << "must be defined";
    CDR_CHECK(payment_date_shift_.has_value()) << "must be defined";
    CDR_CHECK(stub_.has_value()) << "must be defined";
    CDR_CHECK(adjustment_.has_value()) << "must be defined";
    CDR_CHECK(notional_.has_value()) << "must be defined";
    CDR_CHECK(paying_fix_.has_value()) << "must be defined";
    CDR_CHECK(!jur.empty()) << "must be non-empty";

    CDR_CHECK(fixed_freq_->number > 0) << "must be positive";
    CDR_CHECK(float_freq_->number > 0) << "must be positive";

    IrsContract result(fixed_rate_.value_or(Percent::Zero()), *paying_fix_);
    std::vector<IrsPaymentPeriod> sched;

    // --------- fixed leg ------------

    DateType first_payment_period_start = hs.AdvanceDateByBusinessDays(jur, *trade_date_, *start_shift_);
    IrsPaymentPeriod last_payment_period;
    last_payment_period.bounds_.until = hs.AdvanceDateByConvention(jur, first_payment_period_start, *fixed_term_, rule);
    DateType aux_date = hs.AdvanceDateByTenor(first_payment_period_start, *fixed_term_);
    IrsPaymentPeriod period = last_payment_period;
    auto tenor = *fixed_freq_;
    tenor.number *= -1;
    do {
        sched.push_back(period);
        period.bounds_.until = hs.AdvanceDateByTenor(aux_date, tenor);
        period.bounds_.until = hs.AdjustWorkDay(jur, period.bounds_.until, rule);
        tenor.number -= fixed_freq_->number;
    } while (period.bounds_.until > first_payment_period_start);

    if (period.bounds_.until == first_payment_period_start || *stub_ == IrsContract::Stub::SHORT) {
        CDR_CHECK(sched.size() >= 1) << "must be more periods";
        sched.back().bounds_.since = first_payment_period_start;
    } else {
        CDR_CHECK(sched.size() >= 2) << "must be more periods";
        sched.pop_back();
        sched.back().bounds_.since = first_payment_period_start;
    }
    std::reverse(sched.begin(), sched.end());

    for (u32 i = 0; i < sched.size() - 1; ++i) {
        sched[i+1].bounds_.since = sched[i].bounds_.until;
        sched[i].settlement_date_ = hs.AdvanceDateByBusinessDays(jur, sched[i].bounds_.until, *payment_date_shift_);
    }
    sched.back().settlement_date_ = hs.AdvanceDateByBusinessDays(jur, sched.back().bounds_.until, *payment_date_shift_);

    // --------- float leg ------------

    u32 float_begin = ssize(sched);
    first_payment_period_start = hs.AdvanceDateByBusinessDays(jur, *trade_date_, *start_shift_);
    last_payment_period.bounds_.until = hs.AdvanceDateByConvention(jur, first_payment_period_start, *float_term_, rule);
    aux_date = hs.AdvanceDateByTenor(first_payment_period_start, *float_term_);
    period = last_payment_period;
    tenor = *float_freq_;
    tenor.number *= -1;
    do {
        sched.push_back(period);
        period.bounds_.until = hs.AdvanceDateByTenor(aux_date, tenor);
        period.bounds_.until = hs.AdjustWorkDay(jur, period.bounds_.until, rule);
        tenor.number -= float_freq_->number;
    } while (period.bounds_.until > first_payment_period_start);

    if (period.bounds_.until == first_payment_period_start || *stub_ == IrsContract::Stub::SHORT) {
        CDR_CHECK(sched.size() >= float_begin + 1) << "must be more periods";
        sched.back().bounds_.since = first_payment_period_start;
    } else {
        CDR_CHECK(sched.size() >= float_begin + 2) << "must be more periods";
        sched.pop_back();
        sched.back().bounds_.since = first_payment_period_start;
    }
    std::reverse(sched.begin() + float_begin, sched.end());

    for (u32 i = 0; i < sched.size() - 1; ++i) {
        sched[i+1].bounds_.since = sched[i].bounds_.until;
        sched[i].settlement_date_ = hs.AdvanceDateByBusinessDays(jur, sched[i].bounds_.until, *payment_date_shift_);
    }
    sched.back().settlement_date_ = hs.AdvanceDateByBusinessDays(jur, sched.back().bounds_.until, *payment_date_shift_);

    // -------------------------------

    result.jurisdiction_ = jur;
    result.payment_periods_ = std::move(sched);
    result.fixed_leg_ = result.payment_periods_.data();
    result.float_leg_ = result.payment_periods_.data() + float_begin;
    result.adjustment_ = *adjustment_;
    result.notional_ = *notional_;
    // result.chrono_last_idx_ = last;

    Reset();
    return result;
}

void IrsBuilderExperimental::Reset() {
    trade_date_.reset();
    start_shift_.reset();
    fixed_term_.reset();
    float_term_.reset();
    fixed_freq_.reset();
    float_freq_.reset();
    payment_date_shift_.reset();
    stub_.reset();
    fixed_rate_.reset();
    adjustment_.reset();
    notional_.reset();
    paying_fix_.reset();
}

} // namespace cdr
