#include <cdr/swaps/irs.h>

#include <queue>
#include <utility>
#include <cdr/base/check.h>
#include <cdr/calendar/date.h>

namespace cdr {

[[nodiscard]] IrsContract IrsBuilder::Build(const HolidayStorage& hs, const std::string& jur, Adjustment adj) {

    CDR_CHECK(maturity_date_.has_value()) << "maturity_date must be defined";
    CDR_CHECK(effective_date_.has_value()) << "effective_date must be defined";
    CDR_CHECK(cpn_.has_value()) << "coupon must be defined";
    CDR_CHECK(notional_.has_value()) << "notional must be defined";
    CDR_CHECK(paying_fix_.has_value()) << "paying_fix must be defined";

    static constexpr u32 kRandomReservationConstant = 10;
    IrsContract result(cpn_.value(), paying_fix_.value());

    std::vector<IrsPaymentPeriod> sched;
    sched.reserve(kRandomReservationConstant);
    auto period = Period(effective_date_.value(), maturity_date_.value());
    auto compare_dates = [&sched](u32 left_idx, u32 right_idx) {
        return sched[left_idx].Since() > sched[right_idx].Since();
    };
    std::priority_queue<u32, std::vector<u32>, decltype(compare_dates)> chronological_order(compare_dates);

    f64 fixed_payment = 0.0; // TODO: compute
    u8 idx = 0;

    std::optional<DateType> since = std::nullopt;
    std::optional<DateType> due = std::nullopt;

    for (DateType date : hs.BuisnessDays(period.WithFrequency(fixed_freq_.value()), jur, adj)) {
        since = std::exchange(due, date);

        if (!since) [[unlikely]] {
            continue;
        }

        sched.emplace_back(Period{since.value(), due.value()}, fixed_payment);
        chronological_order.push(idx++);
    }
    since = std::exchange(due, effective_date_);
    sched.emplace_back(Period{since.value(), due.value()}, fixed_payment);
    chronological_order.push(idx++);

    since = std::nullopt;
    due = std::nullopt;
    u64 fixed_last = sched.size();

    for (DateType date : hs.BuisnessDays(period.WithFrequency(float_freq_.value()), jur, adj)) {
        since = std::exchange(due, date);

        if (!since) [[unlikely]] {
            continue;
        }

        sched.emplace_back(Period{since.value(), due.value()});
        chronological_order.push(idx++);
    }
    since = std::exchange(due, effective_date_);
    sched.emplace_back(Period{since.value(), due.value()});
    chronological_order.push(idx++);

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

    return result;
}

} // namespace cdr
