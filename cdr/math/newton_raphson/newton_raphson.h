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
    const f64 delta = std::exp2(-20);
    return (target(point + delta) - target(point - delta)) / (delta * 2);
}

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
std::optional<f64> NewtonRaphsonBisectionHybrid(const Fun& target, f64 left_bound, f64 right_bound,
                                                std::optional<f64> start_point = std::nullopt) {
    constexpr u32 kMaxIter = 1000;
    constexpr f64 kXTol = 1e-12;
    constexpr f64 kFTol = 1e-12;
    constexpr f64 kMinDeriv = 1e-14;

    if (left_bound == right_bound) [[unlikely]] {
        return left_bound;
    }
    if (left_bound > right_bound) {
        std::swap(left_bound, right_bound);
    }

    f64 f_left = target(left_bound);
    f64 f_right = target(right_bound);

    if (std::abs(f_left) <= kFTol) {
        return left_bound;
    }
    if (std::abs(f_right) <= kFTol) {
        return right_bound;
    }

    if (std::signbit(f_left) == std::signbit(f_right)) [[unlikely]] {
        return std::nullopt;
    }

    f64 x = start_point.value_or(std::midpoint(left_bound, right_bound));
    x = std::clamp(x, left_bound, right_bound);
    f64 old_x = right_bound;
    f64 last_step = right_bound - left_bound;

    for (u32 iter = 0; iter < kMaxIter; iter++) {
        f64 df_x = Derivative(target, x);
        f64 f_x = target(x);

        if (std::abs(f_x) <= kFTol) {
            return x;
        }

        if (std::signbit(f_left) == std::signbit(f_x)) {
            left_bound = x;
            f_left = f_x;
        } else {
            right_bound = x;
            f_right = f_x;
        }

        f64 x_next = std::midpoint(left_bound, right_bound);
        [[maybe_unused]] bool use_newton = true;

        if (std::abs(df_x) < kMinDeriv) {
            use_newton = false;
        } else {
            f64 newton_step = f_x / df_x;
            f64 x_newton = x - newton_step;

            bool decreasing_too_slow = std::abs(2.0 * f_x) > std::abs(last_step * df_x);
            bool outside_interval = x_newton <= left_bound || x_newton >= right_bound;

            if (decreasing_too_slow || outside_interval) {
                use_newton = false;
            } else {
                x_next = x_newton;
            }
        }

        old_x = x;
        last_step = std::abs(x_next - x);
        x = x_next;

        if (std::abs(x - old_x) <= kXTol) {
            return x;
        }

        if (std::abs(right_bound - left_bound) <= kXTol) {
            return std::midpoint(left_bound, right_bound);
        }
    }

    return std::nullopt;
}

template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
std::optional<f64> NewtonRaphson(const Fun& target, f64 left_bound, f64 right_bound,
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


template <typename Fun>
requires std::invocable<Fun, f64> && std::same_as<std::invoke_result_t<Fun, f64>, f64>
std::optional<f64> FindRoot(const Fun& target, f64 left_bound, f64 right_bound,
                             std::optional<f64> start_point = std::nullopt) {
    return NewtonRaphsonBisectionHybrid(target, left_bound, right_bound, start_point);
}

}  // namespace cdr