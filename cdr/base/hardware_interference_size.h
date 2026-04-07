#pragma once

#include <cstddef>
#include <new>

namespace cdr {

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t hardware_destructive_interference_size = 64;
inline constexpr std::size_t hardware_constructive_interference_size = 64;
#endif

inline constexpr std::size_t kHardwareDestructiveInterferenceSize = hardware_destructive_interference_size;

inline constexpr std::size_t kHardwareConstructiveInterferenceSize = hardware_destructive_interference_size;

}  // namespace cdr