#pragma once

#include <cdr/calendar/date.h>
#include <cdr/types/integers.h>
#include <cdr/types/percent.h>
#include <cdr/calendar/freq.h>
#include <cdr/calendar/holiday_storage.h>
#include <cdr/base/check.h>
#include <cdr/math/newton_raphson/newton_raphson.h>
#include <cdr/curve/internal/export.h>
#include <cdr/market/context.h>
#include <cdr/fx/fx.h>

#include <functional>
#include <map>
#include <tuple>

namespace cdr {

class Curve;
class CurveBuilder;
using DateType = std::chrono::year_month_day;

template <typename T>
concept Contract = requires(T obj, Curve* curve) {
    { std::as_const(obj).SettlementDate() } -> std::same_as<DateType>;
    { obj.ApplyCurve(curve) } -> std::same_as<void>;
    { std::as_const(obj).NPV(curve) } -> std::same_as<std::optional<f64>>;
};

class [[nodiscard]] CDR_CURVE_EXPORT Curve final {
public:
    using PointsContainer = std::map<DateType, Percent>;

    friend class CurveBuilder;
public:
    Curve(MarketContext& ctx): ctx_(ctx) {};

    template <std::input_iterator It>
    Curve(It begin, It end) {
        auto it = points_.begin();
        for (; begin != end; ++begin) {
            const auto& [date, value] = *begin;
            it = points_.emplace_hint(it, date, value);
        }
    }

    Curve(const Curve&) = delete;
    Curve& operator=(const Curve&) = delete;

    struct CurveEasyInit {
        CurveEasyInit& operator()(DateType date, Percent value) {
            target_->Insert(date, value);
            return *this;
        }

        CurveEasyInit& SetToday(DateType date) {
            CDR_CHECK(date.ok()) << "invalid date " << date;
            target_->today_ = date;
            return *this;
        }

        CurveEasyInit& SetCalendar(HolidayStorage* hs) {
            CDR_CHECK(hs != nullptr) << "calendar should be defined";
            target_->calendar_ = hs;
            return *this;
        }

        CurveEasyInit& SetJurisdiction(const std::string& jur) {
            CDR_CHECK(!jur.empty()) << "jurisdiction should be non-empty";
            target_->jurisdiction_ = jur;
            return *this;
        }

        Curve* target_;
    };

    CurveEasyInit StaticInit();

    void Clear();

    template <typename Interpolation, typename... Args>
    [[nodiscard]] Percent Interpolated(DateType date, Args&&... args) const {
        if constexpr (Interpolation::kStatefulImplementation) {
            auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

            static_assert(sizeof...(Args) >= 1, "Statefull interpolations must provide state as argument after date");
            auto&& state = std::get<0>(args_tuple);

            if constexpr (sizeof...(Args) > 1) {
                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    return state.Interpolate(points_, date,
                                             std::get<Is + 1>(std::forward_as_tuple(std::forward<Args>(args)...))...);
                }(std::make_index_sequence<sizeof...(Args) - 1>{});
            } else {
                return state.Interpolate(points_, date);
            }
        } else {
            return Interpolation::Interpolate(points_, date, std::forward<Args>(args)...);
        }
    }


    template <typename Interpolation, typename Transformation, typename... Args>
    [[nodiscard]] inline decltype(auto) InterpolatedTransformed(DateType dt, Transformation&& transform, Args&&... args) {
        Percent interpolated_value = Interpolated<Interpolation>(dt, std::forward<Args>(args)...);
        return std::invoke(std::forward<Transformation>(transform), interpolated_value);
    }

    template <Contract T>
    void AdaptToContract(T* contract) {
        constexpr f64 precision = 0.001;
        DateType settlement = contract->SettlementDate();
        Period period{Today(), settlement};

        Percent left_df = Percent::FromFraction(0.);
        Percent right_df = Percent::FromFraction(1.);
        // TODO: ensure target function grows from 0 to 1

        PointsContainer::iterator node;
        if (auto iter = points_.lower_bound(settlement);
            iter == points_.end() || iter->first != settlement) [[likely]] {
            node = points_.emplace_hint(iter, settlement, Percent::Zero());
        } else {
            node = iter;
        }

        auto target = [&](f64 x) {
            auto df = Percent::FromFraction(x);
            node->second = Curve::DiscountToZeroRates(period, df);
            contract->ApplyCurve(this);
            auto npv = contract->NPV(this);
            CDR_CHECK(npv.has_value()) << "must have value";
            return *npv;
        };
        do {
            Percent mid_df = (left_df + right_df) / 2.;
            f64 npv = target(mid_df.Fraction());

            if (npv < 0) {
                left_df = mid_df;
            } else {
                right_df = mid_df;
            }
        } while ((right_df - left_df).Fraction() > precision);
        // TODO add check left_df > 0 ...
        FindRoot(target, left_df.Fraction(), right_df.Fraction(), std::nullopt);
    }

    void ApplyFXContract(const Curve& other, const ForwardContract& fwd) noexcept;

    // Advance current date and all pillars by one buisness day
    void RollForward() noexcept;

    [[nodiscard]] const DateType& Today() const noexcept {
        return ctx_.Today();
    }

    [[nodiscard]] const HolidayStorage& Calendar() const noexcept {
        return ctx_.Calendar();
    }

    [[nodiscard]] const PointsContainer& Pillars() const noexcept {
        return points_;
    }

    [[nodiscard]] static Percent ZeroRatesToDiscount(const Period& period, Percent p);
    [[nodiscard]] static Percent DiscountToZeroRates(const Period& period, Percent p);

private:

    void Insert(DateType when, Percent value);

private:
    PointsContainer points_;
    std::string jurisdiction_;
    MarketContext& ctx_;
};

class CurveBuilder {
public:
    CurveBuilder(MarketContext& ctx): ctx_(ctx) {};

    template <std::input_iterator Iter>
    Curve FromContracts(Iter begin, Iter end) {
        CDR_CHECK(jusrisdiction_.has_value()) << "Jusrisdiction should be set";

        Curve curve;
        curve.ctx_ = ctx_;
        curve.jurisdiction_ = std::move(*jurisdiction_);

        for (auto i = begin; i < end; i++) {
            curve.AdaptToContract(&(*i));
        }

        return curve;
    }

    template <std::input_iterator Iter>
    Curve FromOther(const Curve& other, Iter begin, Iter end) {
        CDR_CHECK(&ctx_ == &other.ctx_) << "Curves should share market context";
        CDR_CHECK(jurisdiction_.has_value()) << "Jusrisdiction should be set";
        CDR_CHECK(jurisdiction_.value() != other.jurisdiction_) << "Same jurisdiction";

        Curve curve(ctx_);
        curve.jurisdiction_ = std::move(*jurisdiction_);

        for (auto i = begin; i < end; i++) {
            curve.ApplyFXContract(other, *i);
        }
    }

private:
    std::optional<std::string> jurisdiction_;
    MarketContext& ctx_;
};

}  // namespace cdr
