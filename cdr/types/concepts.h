#pragma once
#include <concepts>

namespace cdr {

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<typename T>
concept NonVoid = !std::is_same_v<T, void>;

}  // namespace cdr
