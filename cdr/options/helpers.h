#pragma once

#include <cdr/types/options.h>
#include <cdr/math/distributions/normal.h>
#include <cdr/math/newton_raphson/newton_raphson.h>

namespace cdr {

struct OptionParams {
    f64 S;            // spot price
    f64 K;            // strike price
    f64 rd;           // domestic interest rate
    f64 rf;           // foreign interest rate
    f64 sigma;        // volatility
    f64 T;            // time to maturity in years
    OptionType type;
};

[[nodiscard]] f64 FxOptionPrice(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    f64 d2 = d1 - sigma * sqrtT;

    f64 df_d = std::exp(-rd * T);
    f64 df_f = std::exp(-rf * T);

    if (type == OptionType::CALL) {
        return S * df_f * NormalCDF(d1) - K * df_d * NormalCDF(d2);
    } else {
        return K * df_d * NormalCDF(-d2) - S * df_f * NormalCDF(-d1);
    }
}

[[nodiscard]] f64 FxOptionDelta(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);

    f64 df_f = std::exp(-rf * T);

    if (type == OptionType::CALL) {
        return df_f * NormalCDF(d1);
    } else {
        return df_f * (NormalCDF(d1) - 1.);
    }
}

[[nodiscard]] Percent FxOptionDeltaInPercents(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {
    f64 price = FxOptionPrice(S, K, rd, rf, sigma, T, type);
    f64 delta = FxOptionDelta(S, K, rd, rf, sigma, T, type);
    return Percent::FromFraction(delta * S / price);
}

[[nodiscard]] f64 FxOptionStrikeFromDelta(
    f64 S,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type,
    f64 delta
) noexcept {
    f64 sqrtT = std::sqrt(T);

    f64 adj_delta = delta * std::exp(rf * T);

    f32 sgn = (type == OptionType::CALL) ? 1. : -1.;

    f64 d1 = sgn * NormalCDFInverse(sgn * adj_delta);

    f64 K = S * std::exp(
        -d1 * sigma * sqrtT +
        (rd - rf + 0.5 * sigma * sigma) * T
    );

    return K;
}

[[nodiscard]] f64 FxOptionGamma(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    [[maybe_unused]] OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    f64 df_f = std::exp(-rf * T);
    return df_f * NormalPDF(d1) / (S * sigma * sqrtT);
}

[[nodiscard]] Percent FxOptionGammaInPercents(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    [[maybe_unused]] OptionType type
) noexcept {
    f64 gamma = FxOptionGamma(S, K, rd, rf, sigma, T, type);
    return Percent::FromFraction(gamma * S * 0.01);
}

[[nodiscard]] f64 FxOptionVega(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    [[maybe_unused]] OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    f64 df_f = std::exp(-rf * T);
    return df_f * NormalPDF(d1) * S * sqrtT;
}

[[nodiscard]] Percent FxOptionVegaInPercents(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    [[maybe_unused]] OptionType type
) noexcept {
    f64 vega = FxOptionVega(S, K, rd, rf, sigma, T, type);
    return Percent::FromFraction(vega * sigma * 0.1);
}

[[nodiscard]] f64 FxOptionTheta(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    f64 d2 = d1 - sigma * sqrtT;
    f64 df_d = std::exp(-rd * T);
    f64 df_f = std::exp(-rf * T);

    f32 sgn = (type == OptionType::CALL) ? 1. : -1.;
    return -S * df_f * NormalPDF(d1) * sigma / (2. * sqrtT)
        - sgn * S * df_f * NormalCDF(sgn * d1)
        - sgn * rd * K * df_d * NormalCDF(sgn * d2);
}

[[nodiscard]] f64 FxOptionRho(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) noexcept {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    f64 d2 = d1 - sigma * sqrtT;
    f64 df_d = std::exp(-rd * T);
    f64 df_f = std::exp(-rf * T);

    f32 sgn = (type == OptionType::CALL) ? 1. : -1.;
    return sgn * T * K * df_d * NormalCDF(sgn * d2);
}

[[nodiscard]] f64 FxOptionSigmaFromPrice(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 T,
    OptionType type,
    f64 price
) noexcept {
    auto target = [=](f64 sigma) {
        f64 npv = FxOptionPrice(S, K, rd, rf, sigma, T, type);
        return npv - price;
    };

    f64 res = FindRoot(target, 1e-6, 5.0, std::nullopt)
        .OrCrashProgram() << "Failed to find implied volatility";
    return res;
}

}  // namespace cdr
