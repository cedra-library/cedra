#pragma once

#include <cdr/types/types.h>
#include <cdr/base/check.h>
#include <cdr/math/internal/export.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <numeric>

namespace cdr {

constexpr f64 NormalCDFHartAlgorithm(f64 x) noexcept {
    f64 y = std::abs(x);
    if (y > 37.) {
        return x > 0. ? 1. : 0.;
    }
    f64 cnd;
    f64 sumA = 0.;
    f64 sumB = 0.;
    f64 exp_part = std::exp(-y * y / 2.);
    if (y < 7.07106781186547) {
        sumA = 0.0352624965998911 * y + 0.700383064443688;
        sumA = sumA * y + 6.37396220353165;
        sumA = sumA * y + 33.912866078383;
        sumA = sumA * y + 112.079291497871;
        sumA = sumA * y + 221.213596169931;
        sumA = sumA * y + 220.206867912376;

        sumB = 0.0883883476483184 * y + 1.75566716318264;
        sumB = sumB * y + 16.064177579207;
        sumB = sumB * y + 86.7807322029461;
        sumB = sumB * y + 296.564248779674;
        sumB = sumB * y + 637.333633378831;
        sumB = sumB * y + 793.826512519948;
        sumB = sumB * y + 440.413735824752;

        cnd = exp_part * sumA / sumB;
    } else {
        sumA = y + 0.65;
        sumA = y + 4 / sumA;
        sumA = y + 3 / sumA;
        sumA = y + 2 / sumA;
        sumA = y + 1 / sumA;

        cnd = exp_part / (sumA * 2.506628274631);
    }
    return x > 0. ? 1. - cnd : cnd;
}

constexpr f64 NormalCDFInverseMoroAlgorithm(f64 u) noexcept {
    constexpr std::array<f64, 4> a{2.50662823884, -18.61500062529, 41.39119773534, -25.44106049637};
    constexpr std::array<f64, 4> b{-8.4735109309, 23.08336743743, -21.06224101826, 3.13082909833};
    constexpr std::array<f64, 9> c{0.337475482272615, 0.976169019091719, 0.160797971491821,
                                   0.0276438810333863, 0.0038405729373609, 0.0003951896511919,
                                   3.21767881767818e-05, 2.888167364e-07, 3.960315187e-07};
    f64 r;
    f64 x = u - 0.5;
    if (std::abs(x) < 0.92) {
        r = x * x;
        r = x * (((a[3] * r + a[2]) * r + a[1]) * r + a[0])
            / ((((b[3] * r + b[2]) * r + b[1]) * r + b[0]) * r + 1);
        return r;
    }

    r = x < 0 ? u : 1 - u;

    r = std::log(-std::log(r));
    r = c[0] + r * (c[1] + r * (c[2] + r * (c[3] + r + (c[4] +
        r * (c[5] + r * (c[6] + r * (c[7] + r * c[8])))))));
    if (x < 0) {
        r *= -1;
    }
    return r;
}

[[nodiscard("pure")]] constexpr f64 NormalCDF(f64 x) noexcept {
    return NormalCDFHartAlgorithm(x);
}

[[nodiscard("pure")]] constexpr f64 NormalCDFInverse(f64 u) noexcept {
    return NormalCDFInverseMoroAlgorithm(u);
}

}  // namespace cdr