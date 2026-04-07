#pragma once

#include <cdr/base/internal/generator_impl.h>

namespace cdr {

template<typename T, typename Err = cdr::Error>
using Generator = internal::Generator<T, Err>;

} // namespace cdr
