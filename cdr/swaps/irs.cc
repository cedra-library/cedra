#include <cdr/swaps/irs.h>

#include <queue>
#include <utility>
#include <cdr/base/check.h>
#include <cdr/calendar/date.h>

namespace cdr {

[[nodiscard]] IrsContract IrsBuilder::Build(const HolidayStorage& hs, const std::string& jur, Adjustment adj) {

    CDR_CHECK(maturity_date_.has_value()) << "must be defined";
    CDR_CHECK(settlement_date_.has_value()) << "must be defined";
    CDR_CHECK(cpn_.has_value()) << "must be defined";
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

    for (DateType date : hs.BuisnessDays(period.WithFrequency(fixed_freq_.value()), jur, adj)) {
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

    for (DateType date : hs.BuisnessDays(period.WithFrequency(float_freq_.value()), jur, adj)) {
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

    result.chrono_last_idx_ = last;

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
    cpn_ = std::nullopt;
    notional_ = std::nullopt;
    paying_fix_ = std::nullopt;
}

} // namespace cdr
