#pragma once

#include <cdr/model/internal/export.h>
#include <cdr/base/check.h>
#include <cdr/types/types.h>
#include <cdr/calendar/date.h>
#include <cdr/market/context.h>
#include <cdr/fx/fx.h>
#include <cdr/curve/curve.h>
#include <cdr/swaps/irs.h>

#include <memory>

namespace cdr {

class CDR_MODEL_EXPORT Model {
public:
    // main -> dependent
    using DependencyGraph = std::map<JurisdictionType, std::vector<JurisdictionType>>;
    using CurveStorage = std::map<JurisdictionType, std::unique_ptr<Curve>>;
public:
    Model(MarketContext& ctx): ctx_(ctx) {};

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    [[nodiscard]] MarketContext& Context() noexcept {
        return ctx_;
    }

    [[nodiscard]] DateType Today() const noexcept {
        return ctx_.Today();
    }

    // returns nullptr is curve is not present
    [[nodiscard]] const Curve* GetCurve(const JurisdictionType& jur) const noexcept;
    // returns nullptr is curve is not present
    [[nodiscard]] Curve* GetCurve(const JurisdictionType& jur) noexcept;

    // insert or assign curve to the model
    void AddCurve(std::unique_ptr<Curve>&& curve);
    void AddDependency(const JurisdictionType& main, const JurisdictionType& dependent);
    void SetSwaps(JurisdictionType jur, std::vector<IrsContract>&& swaps) noexcept;
    void SetForwards(JurisdictionType jur, std::vector<ForwardContract>&& fwds) noexcept;

    [[nodiscard]] Expect<void, Error> BuildMainCurve(JurisdictionType jur) noexcept;
    [[nodiscard]] Expect<void, Error> BuildDependentCurve(JurisdictionType main_jur,
                                                          JurisdictionType dependent_jur) noexcept;


    [[nodiscard]] f64 ForwardPrice(const FXPair& pair, const DateType& trade_date) const noexcept;

    void OnNextDay() noexcept {
        for (auto& [jur, curve] : curves_) {
            if (ctx_.Calendar().IsBusinessDay(jur, ctx_.Today())) {
                curve->RollForward();
            }
        }
    }

private:
    std::map<JurisdictionType, std::vector<IrsContract>> swaps_;
    std::map<JurisdictionType, std::vector<ForwardContract>> forwards_;
    CurveStorage curves_;
    DependencyGraph curve_deps_;
    MarketContext& ctx_;
};

}  // namespace cdr
