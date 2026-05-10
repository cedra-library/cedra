#include <cdr/options/interpolation/quadratic_spline.h>

#include <algorithm>
#include <memory>


namespace cdr {

/* static */
Expect<void, Error> QuadraticSplineInterpolator::InitState(void* ptr, std::span<const f64> xs, std::span<const f64> ys) noexcept {
    // --- Quadratic Spline Construction (C1 Continuity) ---
    // Convert raw points into a cacheable analytical form: f(dx) = smile*dx^2 + skew*dx + base_level
    const size_t size = xs.size();

    auto* c_row = static_cast<SplineCoefficients*>(ptr);
    std::uninitialized_default_construct_n(c_row, size);

    // Guard: A spline cannot be constructed with a single strike point
    if (size == 1) {
        c_row[0] = {0., 0., ys[0]};
        return Ok();
    }

    // Initialize the boundary condition: the derivative (slope) at the first node.
    // We use the slope of the first chord as a reasonable starting approximation.
    f64 h0 = xs[1] - xs[0];
    f64 current_slope = (ys[1] - ys[0]) / h0;

    // Iterate through each interval [s, s+1] to "stitch" parabolas together
    for (u64 s = 0; s < size - 1; ++s) {
        const f64 x0 = xs[s];
        const f64 x1 = xs[s + 1];
        const f64 y0 = ys[s];
        const f64 y1 = ys[s + 1];
        const f64 h = x1 - x0;

        // base_level: The function value at the left node of the interval
        c_row[s].base_level = y0;

        // skew: The incoming slope (derivative) for this interval.
        // Passing this value from the previous segment ensures C1 continuity (no kinks).
        c_row[s].skew = current_slope;

        // smile: The curvature of the parabola. Derived algebraically to ensure
        // the parabola intersects the right node (y1) exactly.
        c_row[s].smile = (y1 - y0 - current_slope * h) / (h * h);

        // "Slope Handoff": Calculate the derivative at the end of the current interval (at x1).
        // This becomes the 'skew' for the next segment. Formula: f'(dx) = 2*a*dx + b
        current_slope = 2.0 * c_row[s].smile * h + c_row[s].skew;
    }

    // Finalize the last node.
    // While extrapolation is blocked in Volatility(), we populate the last node
    // for memory consistency, essentially continuing the last slope as a line.
    c_row[size - 1] = {0.0, current_slope, ys[size - 1]};
    return Ok();
}

/* static */
Expect<f64, Error> QuadraticSplineInterpolator::Evaluate(const void* ptr, std::span<const f64> xs, f64 x) noexcept {
    const size_t size = xs.size();
    auto* c_row = static_cast<const SplineCoefficients*>(ptr);

    if (size == 0) [[unlikely]] {
        return ErrorNoData();
    }
    if (x < xs.front() || x > xs.back()) [[unlikely]] {
        return ErrorExtrapolationNotAllowed();
    }

    auto x_idx = std::ranges::lower_bound(xs, x) - xs.begin();

    const auto& coeffs = c_row[x_idx];
    const f64 dx = x - xs[x_idx];
    const f64 res = (coeffs.smile * dx + coeffs.skew) * dx + coeffs.base_level;
    return Ok(res);
}

}
