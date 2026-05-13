#pragma once

#include <cdr/options/internal/export.h>
#include <cdr/types/errors.h>
#include <cdr/types/types.h>

#include <span>

namespace cdr {

class CDR_OPTIONS_EXPORT SABRInterpolator {
public:
    struct SabrParameters {
        static constexpr f64 kBeta = 1.0;
        f64 alpha;
        f64 rho;
        f64 nu;
        f64 F;  // Forward price
        f64 T;  // Time to maturity
    };

    using StrikeVolatilityType = SabrParameters;
    using DeltaVolatilityType = f64;

public:
    static constexpr size_t StateRequiredMemorySize(size_t /*n*/) noexcept {
        // SABR needs only one structure of parameters per date
        return sizeof(SabrParameters);
    }

    static constexpr size_t StateRequiredMemoryAlignment(size_t /*n*/) noexcept {
        return alignof(SabrParameters);
    }

    // Hagan approximation, implemented as template to support double and ceres::Jet
    // Uses the Hagan approximation for the SABR model with Beta = 1.0
    //
    // Formula: sigma(K) = alpha * (z / chi(z)) * [1 + correction_term * T]
    // where:
    //   z = (nu / alpha) * log(F / K)
    //   chi(z) = log((sqrt(1 - 2*rho*z + z^2) + z - rho) / (1 - rho))
    //
    // Note: Handle K = F (ATM) case separately to avoid 0/0 singularity,
    // resulting in: sigma_atm = alpha * [1 + correction_term * T].
    template <typename T>
    static T CalculateHagan(T K, T F, T Time, T alpha, T rho, T nu) {
        using std::log;
        using std::sqrt;
        using std::abs;
        using std::pow;

        if (K <= T(0.0) || F <= T(0.0)) {
            return T(0.0);
        }

        constexpr f64 beta = SabrParameters::kBeta;
        constexpr f64 one_minus_beta = 1.0 - beta;
        constexpr f64 one_minus_beta2 = one_minus_beta * one_minus_beta;

        const T f_k_beta = pow(F * K, one_minus_beta / 2.0);
        const T log_f_k = log(F / K);

        T den = f_k_beta * (T(1.0) + (one_minus_beta2 / 24.0) * log_f_k * log_f_k +
                            (one_minus_beta2 * one_minus_beta2 / 1920.0) * pow(log_f_k, 4.0));

        const T z = (nu / alpha) * f_k_beta * log_f_k;

        const T xz = log((sqrt(T(1.0) - T(2.0) * rho * z + z * z) + z - rho) / (T(1.0) - rho));

        const T term2 = T(1.0) + (
            (one_minus_beta2 / 24.0) * (alpha * alpha / pow(f_k_beta, 2.0)) +
            (0.25 * rho * beta * nu * alpha / f_k_beta) +
            ((T(2.0) - T(3.0) * rho * rho) / 24.0) * nu * nu
        ) * Time;

        if (abs(log_f_k) < T(1e-8)) {
            return (alpha / pow(F, one_minus_beta)) * term2;
        }

        return (alpha / den) * (z / xz) * term2;
    }

    [[nodiscard]] static Expect<void, Error> InitState(void* coefs_ptr, std::span<const f64> xs,
                                                       std::span<const f64> ys) noexcept;

    [[nodiscard]] static Expect<f64, Error> Evaluate(const void* coefs_ptr, std::span<const f64> xs, f64 x) noexcept;
};

struct CDR_OPTIONS_EXPORT SabrCostFunctor {
    SabrCostFunctor(f64 strike, f64 market_vol, f64 F, f64 T)
        : strike_(strike), market_vol_(market_vol), F_(F), T_(T)
    {}

    template <typename T>
    bool operator()(const T* const params, T* residual) const {
        T model_vol = SABRInterpolator::CalculateHagan<T>(T(strike_), T(F_), T(T_), params[0], params[1], params[2]);

        residual[0] = model_vol - T(market_vol_);

        return true;
    }

private:
    const f64 strike_;
    const f64 market_vol_;
    const f64 F_;
    const f64 T_;
};

}  // namespace cdr
