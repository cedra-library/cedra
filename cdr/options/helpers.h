#pragma once

#include <cdr/types/options.h>
#include <cdr/math/distributions/normal.h>

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

[[nodiscard]] constexpr f64 FxOptionPrice(
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

[[nodiscard]] constexpr f64 FxOptionDelta(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) {
    f64 sqrtT = std::sqrt(T);
    f64 d1 = (std::log(S / K) + (rd - rf + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);

    f64 df_f = std::exp(-rf * T);

    if (type == OptionType::CALL) {
        return df_f * NormalCDF(d1);
    } else {
        return df_f * (NormalCDF(d1) - 1.);
    }
}

Percent FxOptionDeltaInPercents(
    f64 S,
    f64 K,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type
) {
    f64 price = FxOptionPrice(S=S, K=K, rd=rd, rf=rf, sigma=sigma, T=T, type=type);
    f64 delta = FxOptionDelta(S=S, K=K, rd=rd, rf=rf, sigma=sigma, T=T, type=type);
    return Percent::FromFraction(delta * S / price);
}

f64 FxOptionStrikeFromDelta(
    f64 S,
    f64 rd,
    f64 rf,
    f64 sigma,
    f64 T,
    OptionType type,
    f64 delta
) {
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

}  // namespace cdr
