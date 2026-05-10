#include <cdr/options/interpolation/cubic_spline.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace stdr = std::ranges;

namespace cdr {

/* static */
Expect<void, Error> CubicSplineInterpolator::InitState(void* ptr, std::span<const f64> xs, std::span<const f64> ys) noexcept {
    const size_t n = xs.size();
    if (n < 2 || ys.size() != n) [[unlikely]] {
        return ErrorNoData();
    }
    auto* c_row = static_cast<SplineCoefficients*>(ptr);
    std::uninitialized_default_construct_n(c_row, n);

    for (size_t i = 0; i < n; ++i) {
        c_row[i].a = ys[i];
    }

    std::vector<f64> h(n - 1);
    for (size_t i = 0; i < n - 1; i++) {
        h[i] = xs[i + 1] - xs[i];
        if (h[i] <= 0) [[unlikely]] {
            // x values must be strictly increasing
            return ErrorNoData();
        }
    }

    std::vector<f64> alpha(n - 1);
    for (size_t i = 1; i < n - 1; i++) {
        alpha[i] = (3 / h[i]) * (ys[i + 1] - ys[i]) - (3 / h[i - 1]) * (ys[i] - ys[i - 1]);
    }

    std::vector<f64> l(n), mu(n), z(n);
    l[0] = 1.;
    mu[0] = z[0] = 0.;

    for (size_t i = 1; i < n - 1; i++) {
        l[i] = 2 * (xs[i + 1] - xs[i - 1]) - h[i - 1] * mu[i - 1];
        if (l[i] == 0) [[unlikely]] {
            return ErrorNoData();
        }
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[n - 1] = 1.;
    z[n - 1] = c_row[n - 1].c = 0.;

    for (size_t j = n - 2; j != static_cast<size_t>(-1); j--) {
        c_row[j].c = z[j] - mu[j] * c_row[j + 1].c;
        c_row[j].b = (ys[j + 1] - ys[j]) / h[j] - h[j] * (c_row[j + 1].c + 2 * c_row[j].c) / 3;
        c_row[j].d = (c_row[j + 1].c - c_row[j].c) / (3 * h[j]);
    }
    return Ok();
}

/* static */
Expect<f64, Error> CubicSplineInterpolator::Evaluate(const void* ptr, std::span<const f64> xs, f64 x) noexcept {
    const size_t n = xs.size();
    auto* c_row = static_cast<const SplineCoefficients*>(ptr);
    if (n == 0) [[unlikely]] {
        return ErrorNoData();
    }
    if (x < xs[0]) [[unlikely]] {
        return ErrorExtrapolationNotAllowed();
    }
    if (x > xs[n - 1]) [[unlikely]] {
        return ErrorExtrapolationNotAllowed();
    }

    auto it = stdr::upper_bound(xs, x);
    size_t i = it - xs.begin() - 1;

    f64 dx = x - xs[i];
    f64 res = c_row[i].a + c_row[i].b * dx + c_row[i].c * dx*dx + c_row[i].d * dx*dx*dx;
    return Ok(res);
}

}
