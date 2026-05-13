#pragma once

#include <cdr/types/types.h>
#include <cdr/types/errors.h>
#include <cdr/options/internal/export.h>
#include <span>

namespace cdr {

class CDR_OPTIONS_EXPORT SABRInterolator {
public:

    struct SabrParameters {
        static constexpr f64 kBeta = 1.0;
        f64 alpha;
        f64 rho;
        f64 nu;
    };


    using StrikeVolatilityType = SabrParameters;
    using DeltaVolatilityType = f64;


public:

    static constexpr size_t StateReqiuiredMemorySize(size_t n) noexcept {
        return n * sizeof(SabrParameters);
    }


    static constexpr size_t StateRequiredMemoryAlignment(size_t n) noexcept {
        return alignof(SabrParameters);
    }

    [[nodiscard]] static Expect<void, Error> InitState(void* coefs_ptr, std::span<const f64> xs, std::span<const f64> ys) noexcept;

    static void DestroyState(void* coefs_ptr, size_t n) noexcept {
        std::destroy_n(static_cast<SabrParameters*>(coefs_ptr), n);
    }

    [[nodiscard]] static Expect<f64, Error> Evaluate(const void* coefs_ptr, std::span<const f64> xs, f64 x) noexcept;


};

} // namespace cdr
