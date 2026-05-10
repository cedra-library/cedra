#pragma once

#include <cdr/types/types.h>
#include <cdr/base/check.h>
#include <cdr/math/internal/export.h>

#include <cmath>
#include <concepts>
#include <algorithm>
#include <numeric>
#include <optional>

namespace cdr {

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
f64 Derivative(const Fun& target, f64 point) {
    const f64 delta = std::exp2(-16);
    return (target(point + delta) - target(point - delta)) / (delta * 2);
}

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
std::optional<f64> FindRoot(const Fun& target, f64 left_bound, f64 right_bound,
                            std::optional<f64> start_point = std::nullopt) {
    constexpr u32 kMaxIter = 1000;
    const f64 tol = std::exp2(-16);
    [[maybe_unused]] u32 iteration = 0;
    if (!start_point.has_value()) {
        start_point.emplace(std::midpoint(left_bound, right_bound));
    }

    f64 x = std::clamp(*start_point, left_bound, right_bound);
    f64 val = target(x);
    while (std::abs(val) > tol && iteration != kMaxIter) {
        f64 deriv = Derivative(target, x);
        if (std::abs(deriv) < 1e-12) {
            return std::nullopt;
        }
        f64 x_next = x - val / deriv;
        x_next = std::clamp(x_next, left_bound, right_bound);
        x = x_next;
        val = target(x);
        iteration++;
    }

    if (iteration == kMaxIter) {
        return std::nullopt;
    }
    return x;
}

}  // namespace cdr