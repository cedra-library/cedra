#include <cdr/model/model.h>
#include <cdr/curve/interpolation/linear.h>

namespace cdr {

void Model::SetSwaps(JurisdictionType jur, std::vector<IrsContract>&& swaps) noexcept {
    swaps_.insert_or_assign(jur, std::move(swaps));
}

void Model::SetForwards(JurisdictionType jur, std::vector<ForwardContract>&& fwds) noexcept {
    forwards_.insert_or_assign(jur, std::move(fwds));
}


Expect<void, Error> Model::BuildMainCurve(JurisdictionType jur) noexcept {
    auto& swaps = swaps_[jur];
    auto curve = CurveBuilder(ctx_)
        .Jurisdiction(jur)
        .FromContracts(swaps.begin(), swaps.end());
    ;
    AddCurve(std::move(curve));
    return Ok();
}


Expect<void, Error> Model::BuildDependentCurve(JurisdictionType main_jur, JurisdictionType dependent_jur) noexcept {
    auto *main = GetCurve(main_jur);
    if (main == nullptr) [[unlikely]] {
        return Failure(Error::NoData);
    }
    const auto& fwds = forwards_[dependent_jur];
    auto curve = CurveBuilder(ctx_)
        .Jurisdiction(dependent_jur)
        .FromOther(*main, fwds.begin(), fwds.end())
    ;
    AddCurve(std::move(curve));

    return Ok();
}


void Model::AddCurve(std::unique_ptr<Curve>&& curve) {
    auto jur = curve->GetJurisdiction();
    curves_.insert_or_assign(jur, std::move(curve));
}

void Model::AddDependency(const JurisdictionType& main, const JurisdictionType& dependent) {
    curve_deps_[main].push_back(dependent);
}

const Curve* Model::GetCurve(const JurisdictionType& jur) const noexcept {
    if (auto it = curves_.find(jur); it == curves_.end()) [[unlikely]] {
        return nullptr;
    } else {
        return it->second.get();
    }
}

Curve* Model::GetCurve(const JurisdictionType& jur) noexcept {
    if (auto it = curves_.find(jur); it == curves_.end()) [[unlikely]] {
        return nullptr;
    } else {
        return it->second.get();
    }
}

f64 Model::ForwardPrice(const FXPair& pair, const DateType& date) const noexcept {
    auto spot = ctx_.FxSpot(pair);
    auto *base_curve = GetCurve(pair.first);
    auto *quote_curve = GetCurve(pair.second);
    if (base_curve == nullptr || quote_curve == nullptr) [[unlikely]] {
        return 0.;
    }
    auto base_rate = base_curve->Interpolated<Linear>(date, ctx_.Calendar(), base_curve->GetJurisdiction());
    auto quote_rate = quote_curve->Interpolated<Linear>(date, ctx_.Calendar(), quote_curve->GetJurisdiction());

    auto base_df = base_curve->ZeroRatesToDiscount(date, base_rate);
    auto quote_df = quote_curve->ZeroRatesToDiscount(date, quote_rate);

    return spot * (base_df / quote_df).Fraction();
}


}  // namespace cdr
