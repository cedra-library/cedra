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
#include <memory>

namespace cdr {

class Curve;
class CurveBuilder;
using DateType = std::chrono::year_month_day;

template <typename T>
concept Contract = requires(T obj, const Curve& curve) {
    { std::as_const(obj).SettlementDate() } -> std::same_as<DateType>;
    { obj.ApplyCurve(curve) } -> std::same_as<void>;
    { std::as_const(obj).NPV(curve) } -> std::same_as<std::optional<f64>>;
};

class [[nodiscard]] CDR_CURVE_EXPORT Curve final {
public:
    using PointsContainer = std::map<DateType, Percent>;

    friend class CurveBuilder;
public:
    Curve(const Curve&) = delete;
    Curve& operator=(const Curve&) = delete;

    [[nodiscard]] static std::unique_ptr<Curve> Create(MarketContextView ctx, const JurisdictionType& jur);

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
    void AdaptToContract(T& contract) {
        constexpr f64 precision = 0.001;
        DateType settlement = contract.SettlementDate();
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
            node->second = DiscountToZeroRates(settlement, df);
            contract.ApplyCurve(*this);
            auto npv = contract.NPV(*this);
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

    [[nodiscard]] DateType Today() const noexcept {
        return ctx_.Today();
    }

    [[nodiscard]] const HolidayStorage& Calendar() const noexcept {
        return ctx_.Calendar();
    }

    [[nodiscard]] const PointsContainer& Pillars() const noexcept {
        return points_;
    }

    [[nodiscard]] JurisdictionType GetJurisdiction() const noexcept {
        return jurisdiction_;
    }

    [[nodiscard]] Percent ZeroRatesToDiscount(const DateType& date, Percent p) const;
    [[nodiscard]] Percent DiscountToZeroRates(const DateType& date, Percent p) const;

private:
    Curve(MarketContextView ctx, JurisdictionType jur)
        : ctx_(ctx)
        , jurisdiction_(std::move(jur))
    {}

    void Insert(DateType when, Percent value);

private:
    PointsContainer points_;
    MarketContextView ctx_;
    JurisdictionType jurisdiction_;
};

class CDR_CURVE_EXPORT CurveBuilder {
public:
    CurveBuilder(MarketContextView ctx): ctx_(ctx) {};

    [[maybe_unused]] CurveBuilder& Jurisdiction(JurisdictionType jur) {
        jurisdiction_.emplace(std::move(jur));
        return *this;
    }

    [[maybe_unused]] CurveBuilder& Add(const DateType& when, Percent value);

    template <std::input_iterator Iter>
    [[nodiscard]] std::unique_ptr<Curve> FromContracts(Iter begin, Iter end) {
        CDR_CHECK(jurisdiction_.has_value()) << "Jusrisdiction should be set";

        auto curve = Curve::Create(ctx_, std::move(*jurisdiction_));

        for (auto i = begin; i < end; i++) {
            curve->AdaptToContract(*i);
        }

        return curve;
    }

    template <std::input_iterator Iter>
    [[nodiscard]] std::unique_ptr<Curve> FromOther(const Curve& other, Iter begin, Iter end) {
        CDR_CHECK(&ctx_ == &other.ctx_) << "Curves should share market context";
        CDR_CHECK(jurisdiction_.has_value()) << "Jusrisdiction should be set";
        CDR_CHECK(jurisdiction_.value() != other.jurisdiction_) << "Same jurisdiction";

        auto curve = Curve::Create(ctx_, std::move(*jurisdiction_));

        for (auto i = begin; i < end; i++) {
            curve->ApplyFXContract(other, *i);
        }

        return curve;
    }

    [[nodiscard]] std::unique_ptr<Curve> FromPoints();

private:
    Curve::PointsContainer points_;
    MarketContextView ctx_;
    std::optional<JurisdictionType> jurisdiction_;
};

}  // namespace cdr
