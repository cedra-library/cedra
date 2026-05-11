#pragma once

#include <cdr/types/types.h>
#include <cdr/options/interpolation/concept.h>
#include <cdr/types/errors.h>
#include <cdr/types/expect.h>

#include <span>
#include <cdr/options/internal/export.h>

namespace cdr {

class CDR_OPTIONS_EXPORT QuadraticSplineInterpolator {
public:
    struct SplineCoefficients {
        f64 smile;
        f64 skew;
        f64 base_level;
    };

    struct State {
        size_t size;
        SplineCoefficients* coefs_ptr;
    };

    using StrikeVolatilityType = SplineCoefficients;
    using DeltaVolatilityType = SplineCoefficients;

public:
    static constexpr size_t StateRequiredMemorySize(size_t n) noexcept {
        return n * sizeof(SplineCoefficients);
    }

    static constexpr size_t StateRequiredMemoryAlignment(size_t n) noexcept {
        return alignof(SplineCoefficients);
    }

    [[nodiscard]] static Expect<void, Error> InitState(void* coefs_ptr,
                                                       std::span<const f64> xs, std::span<const f64> ys) noexcept;

    static void DestroyState(void* coefs_ptr, size_t n) noexcept {
        std::destroy_n(static_cast<SplineCoefficients*>(coefs_ptr), n);
    }

    [[nodiscard]] static Expect<f64, Error> Evaluate(const void* coefs_ptr, std::span<const f64> xs, f64 x) noexcept;
};

}  // namespace cdr
