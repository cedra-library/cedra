#pragma once

#include <cdr/types/types.h>
// #include <cdr/options/interpolation/concept.h>
#include <cdr/types/errors.h>
#include <cdr/types/expect.h>

#include <span>

namespace cdr {

class CubicSplineInterpolator {
    struct SplineCoefficients {
        f64 a;
        f64 b;
        f64 c;
        f64 d;
    };

    struct State {
        size_t size;
        SplineCoefficients* coefs_ptr;
    };

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
