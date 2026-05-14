#pragma once

#include <ceres/jet.h>
#include <cdr/options/greeks.h>
#include <cdr/types/types.h>
#include <cdr/math/distributions/normal.h>

namespace cdr {

namespace internal {

template <typename T>
[[nodiscard]] T FxOptionPriceJet(
    const T& S,
    const T& K,
    const T& rd,
    const T& rf,
    const T& sigma,
    const T& Tm,
    OptionType type
) noexcept {

    T sqrtT = sqrt(Tm);

    T d1 =
        (log(S / K) +
         (rd - rf + T(0.5) * sigma * sigma) * Tm) /
        (sigma * sqrtT);

    T d2 = d1 - sigma * sqrtT;

    T df_d = exp(-rd * Tm);
    T df_f = exp(-rf * Tm);

    if (type == OptionType::CALL) {
        return S * df_f * NormalCDF(d1)
             - K * df_d * NormalCDF(d2);
    } else {
        return K * df_d * NormalCDF(-d2)
             - S * df_f * NormalCDF(-d1);
    }
}

} // namespace internal


[[nodiscard]] inline Greeks ComputeAllGreeksAD(
    f64 S, f64 K, f64 rd, f64 rf, f64 sigma, f64 T, OptionType type
) noexcept {
    using Jet = ceres::Jet<double, 3>;

    Jet Sjet(S, 0);
    Jet sigmaJet(sigma, 1);
    Jet rdJet(rd, 2);

    Jet priceJet = internal::FxOptionPriceJet(
        Sjet, Jet(K), rdJet, Jet(rf), sigmaJet, Jet(T), type
    );

    return {
        priceJet.a,    // Значение цены
        priceJet.v[0], // Дельта (dP/dS)
        priceJet.v[1],  // Вега (dP/dSigma)
        priceJet.v[2]    // Ро (dP/drd)
    };
}

//
// Delta = dPrice / dS
//
[[nodiscard]] inline f64 FxOptionDeltaAD(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {

    using Jet = ceres::Jet<double, 1>;

    Jet Sjet(S);
    Sjet.v[0] = 1.0;

    Jet price = internal::FxOptionPriceJet(
        Sjet,
        Jet(K),
        Jet(rd),
        Jet(rf),
        Jet(sigma),
        Jet(T),
        type
    );

    return price.v[0];
}

//
// Vega = dPrice / dSigma
//
[[nodiscard]] inline f64 FxOptionVegaAD(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {

    using Jet = ceres::Jet<double, 1>;

    Jet sigmaJet(sigma);
    sigmaJet.v[0] = 1.0;

    Jet price = internal::FxOptionPriceJet(
        Jet(S),
        Jet(K),
        Jet(rd),
        Jet(rf),
        sigmaJet,
        Jet(T),
        type
    );

    return price.v[0];
}

//
// Rho = dPrice / drd
//
[[nodiscard]] inline f64 FxOptionRhoAD(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {

    using Jet = ceres::Jet<double, 1>;

    Jet rdJet(rd);
    rdJet.v[0] = 1.0;

    Jet price = internal::FxOptionPriceJet(
        Jet(S),
        Jet(K),
        rdJet,
        Jet(rf),
        Jet(sigma),
        Jet(T),
        type
    );

    return price.v[0];
}

} // namespace cdr
