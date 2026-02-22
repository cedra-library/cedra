#pragma once

#include <cdr/types/types.h>
#include <cdr/base/check.h>

#include <cmath>
#include <concepts>
#include <algorithm>
#include <numeric>

namespace cdr {

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
f64 Derivative(const Fun& target, f64 point)
{
    const f64 delta = std::exp2(-16);
    return (target(point + delta) - target(point - delta)) / (delta * 2);
}

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
std::optional<f64> FindRoot(const Fun& target, f64 left_bound, f64 right_bound, std::optional<f64> start_point)
{
    const f64 tol = std::exp2(-16);
    [[maybe_unused]] u32 iteration = 0;
    if (!start_point.has_value()) {
        start_point.emplace(std::midpoint(left_bound, right_bound));
    }

    f64 x = *start_point;
    f64 val = target(x);
    while (std::abs(val) > tol) {
        f64 x_next = x - val / Derivative(target, x);
        std::clamp(x_next, left_bound, right_bound);
        x = x_next;
        val = target(x);
        iteration++;
    }

    return x;
}

}  // namespace cdr