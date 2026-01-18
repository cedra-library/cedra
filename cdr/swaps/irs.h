#pragma once

#include <limits>
#include <optional>
#include <cdr/types/percent.h>
#include <cdr/calendar/date.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/curve/curve.h>
#include <cdr/types/concepts.h>

namespace cdr {

class IrsBuilder;

class IrsPaymentPeriod final {
public:
    static constexpr u32 kNotInitialized = std::numeric_limits<u32>::max();

    friend class IrsBuilder;

public:

    explicit IrsPaymentPeriod(const Period& bounds, const std::optional<f64>& payment = std::nullopt)
        : bounds_(bounds)
        , settlement_date_(bounds_.Until())
        , payment_(payment)
        , chrono_prev_idx_(kNotInitialized)
        , chrono_next_idx_(kNotInitialized)
    {}

    [[nodiscard]] const DateType& Since() const noexcept {
        return bounds_.Since();
    }

    [[nodiscard]] const DateType& Until() const noexcept {
        return bounds_.Until();
    }

    [[nodiscard]] const DateType& SettlementDate() const noexcept {
        return settlement_date_;
    }

    [[nodiscard]] bool ChronoFirstPayment() const noexcept {
        return chrono_prev_idx_ == kNotInitialized;
    }

    [[nodiscard]] bool ChronoLastPayment() const noexcept {
        return chrono_next_idx_ == kNotInitialized;
    }

    [[nodiscard]] bool HasKnownPayment() const noexcept {
        return payment_.has_value();
    }

    void SetPayment(f64 payment) noexcept {
        payment_ = payment;
    }

    [[nodiscard]] std::optional<f64> Payment() const noexcept {
        return payment_;
    }

private:
    Period bounds_;
    DateType settlement_date_;
    std::optional<f64> payment_;
    u32 chrono_prev_idx_;
    u32 chrono_next_idx_;
};


class IrsContract final {
public:
    friend class IrsBuilder;

    [[nodiscard]] std::span<const IrsPaymentPeriod> FixedLeg() const noexcept {
        return {fixed_leg_, float_leg_};
    }

    [[nodiscard]] std::span<const IrsPaymentPeriod> FloatLeg() const noexcept {
        return {float_leg_, payment_periods_.end().base()};
    }

    [[nodiscard]] DateType GetHorizonDate() const {
        return payment_periods_.front().Since();
    }

    [[nodiscard]] DateType GetMaturityDate() const {
        return payment_periods_.back().Until();
    }

    [[nodiscard]] const Percent& Coupon() const noexcept {
        return coupon_;
    }

    [[nodiscard]] bool PayFix() const noexcept {
        return paying_fix_;
    }

    [[nodiscard]] f64 Notional() const noexcept {
        return notional_;
    }

    [[nodiscard]] DateType SettlementDate() const noexcept {
        CDR_CHECK(!FloatLeg().empty()) << "must be not empty";
        return FloatLeg().back().SettlementDate();
    }

    void ApplyCurve(Curve* curve) noexcept;

    [[nodiscard]] std::optional<f64> PVFixed(Curve *curve) const noexcept;
    [[nodiscard]] std::optional<f64> PVFloat(Curve *curve) const noexcept;
    [[nodiscard]] std::optional<f64> NPV(Curve *curve) const noexcept;

private:

    IrsContract(Percent coupon, bool paying_fix)
        : coupon_(coupon)
        , paying_fix_(paying_fix)
    {}

    [[nodiscard]] std::span<IrsPaymentPeriod> FixedLegMut() const noexcept {
        return {fixed_leg_, float_leg_};
    }

    [[nodiscard]] std::span<IrsPaymentPeriod> FloatLegMut() const noexcept {
        return {float_leg_, payment_periods_.end().base()};
    }

private:
    std::string jurisdiction_;
    std::vector<IrsPaymentPeriod> payment_periods_;
    IrsPaymentPeriod* fixed_leg_ = nullptr;
    IrsPaymentPeriod* float_leg_ = nullptr;
    Percent coupon_;
    Percent adjustment_;
    f64 notional_;
    u32 chrono_start_idx_ = 0;
    u32 chrono_last_idx_ = 0;
    bool paying_fix_ = false;
};

class IrsBuilder final {
public:
    [[maybe_unused]] IrsBuilder& Coupon(Percent p) {
        cpn_ = p;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& SettlementDate(const DateType& ed) {
        settlement_date_ = ed;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& MaturityDate(const DateType& md) {
        maturity_date_ = md;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& EffectiveDate(const DateType& dt) {
        effective_date_ = dt;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& PayFix(bool pf) {
        paying_fix_ = pf;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& FixedFreq(Freq freq) {
        fixed_freq_ = freq;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& FloatFreq(Freq freq) {
        float_freq_ = freq;
        return *this;
    }

    [[maybe_unused]] IrsBuilder& Notion(f64 value) {
        notional_ = value;
        return *this;
    }

    [[nodiscard]] IrsContract Build(const HolidayStorage& hs, const std::string& jur,
                                    DateRollingRule rule = DateRollingRule::kFollowing);


    void Reset();
private:
    std::optional<std::string> jurisdiction_;
    std::optional<DateType> maturity_date_;
    std::optional<DateType> settlement_date_;
    std::optional<DateType> effective_date_;
    std::optional<Freq> fixed_freq_;
    std::optional<Freq> float_freq_;
    std::optional<Percent> cpn_;
    std::optional<Percent> adjustment_;
    std::optional<f64> notional_;
    std::optional<bool> paying_fix_;
};

static_assert(Contract<IrsContract>);

} // namespace cdr

