#pragma once

#include <cdr/types/types.h>
#include <cdr/options/interpolation/concept.h>
#include <cdr/types/errors.h>
#include <cdr/types/expect.h>

#include <span>

namespace cdr {

class QuadraticSplineInterpolator {
    struct SplineCoefficients {
        f64 smile;
        f64 skew;
        f64 base_level;
    };

    struct State {
        size_t size;
        SplineCoefficients* coefs_ptr;
    };

public:
    static constexpr size_t StateRequiredMemory(size_t n) noexcept {
        return n * sizeof(SplineCoefficients);
    }
    static void InitState(void* coefs_ptr, std::span<const f64> xs, std::span<const f64> ys) noexcept;
    static void DestroyState(void* coefs_ptr) noexcept {};

    static Expect<f64, Error> Evaluate(const void* coefs_ptr, std::span<const f64> xs, f64 x) noexcept;
};

}  // namespace cdr
