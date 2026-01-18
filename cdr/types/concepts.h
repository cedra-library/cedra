#pragma once
#include <concepts>
#include <utility>
#include <chrono>

namespace cdr {

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

}  // namespace cdr
