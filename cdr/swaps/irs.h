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
    friend class IrsBuilderExperimental;

public:

    IrsPaymentPeriod() = default;
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
    enum class Stub {SHORT, LONG};
public:
    friend class IrsBuilder;
    friend class IrsBuilderExperimental;

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

    [[nodiscard]] const Percent& FixedRate() const noexcept {
        return fixed_rate_;
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

    // Deprecated. Use cdr/math instead
    [[nodiscard]] std::optional<f64> dPVFixed(Curve *curve, const DateType& date, Percent rate) const noexcept;
    // Deprecated. Use cdr/math instead
    [[nodiscard]] std::optional<f64> dPVFloat(Curve *curve, const DateType& date, Percent rate) const noexcept;
    // Deprecated. Use cdr/math instead
    [[nodiscard]] std::optional<f64> dNPV(Curve *curve, const DateType& date, Percent rate) const noexcept;

private:

    IrsContract(Percent fixed_rate, bool paying_fix)
        : fixed_rate_(fixed_rate)
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
    Percent fixed_rate_;
    Percent adjustment_;
    f64 notional_;
    u32 chrono_start_idx_ = 0;
    u32 chrono_last_idx_ = 0;
    bool paying_fix_ = false;
};

class IrsBuilder final {
public:
    [[maybe_unused]] IrsBuilder& FixedRate(Percent p) {
        fixed_rate_ = p;
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

    [[maybe_unused]] IrsBuilder& Adjustment(Percent adj) {
        adjustment_ = adj;
        return *this;
    }

    [[nodiscard]] IrsContract Build(const HolidayStorage& hs, const std::string& jur,
                                    DateRollingRule rule = DateRollingRule::kFollowing);

    void Reset();

private:
    std::optional<DateType> maturity_date_;
    std::optional<DateType> settlement_date_;
    std::optional<DateType> effective_date_;
    std::optional<Freq> fixed_freq_;
    std::optional<Freq> float_freq_;
    std::optional<Percent> fixed_rate_;
    std::optional<Percent> adjustment_;
    std::optional<f64> notional_;
    std::optional<bool> paying_fix_;
};

class IrsBuilderExperimental {
public:
    [[maybe_unused]] IrsBuilderExperimental& TradeDate(const DateType& trade_date) {
        trade_date_ = trade_date;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& StartShift(u32 shift) {
        start_shift_ = shift;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& FixedTerm(Tenor term) {
        fixed_term_ = term;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& FloatTerm(Tenor term) {
        float_term_ = term;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& FixedFreq(Tenor freq) {
        fixed_freq_ = freq;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& FloatFreq(Tenor freq) {
        float_freq_ = freq;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& PaymentDateShift(u32 shift) {
        payment_date_shift_ = shift;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& Stub(IrsContract::Stub stub) {
        stub_ = stub;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& FixedRate(Percent p) {
        fixed_rate_ = p;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& Adjustment(Percent adj) {
        adjustment_ = adj;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& Notion(f64 value) {
        notional_ = value;
        return *this;
    }

    [[maybe_unused]] IrsBuilderExperimental& PayFix(bool pf) {
        paying_fix_ = pf;
        return *this;
    }

    [[nodiscard]] IrsContract Build(const HolidayStorage& hs, const std::string& jur,
                                    DateRollingRule rule = DateRollingRule::kFollowing);

    void Reset();

private:
    std::optional<DateType> trade_date_;
    std::optional<u32> start_shift_;
    std::optional<Tenor> fixed_term_;
    std::optional<Tenor> float_term_;
    std::optional<Tenor> fixed_freq_;
    std::optional<Tenor> float_freq_;
    std::optional<u32> payment_date_shift_;
    std::optional<IrsContract::Stub> stub_;
    std::optional<Percent> fixed_rate_;
    std::optional<Percent> adjustment_;
    std::optional<f64> notional_;
    std::optional<bool> paying_fix_;
};

static_assert(Contract<IrsContract>);

} // namespace cdr

