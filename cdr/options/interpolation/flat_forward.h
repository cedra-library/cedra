#pragma once

#include <cdr/types/floats.h>
#include <cdr/options/internal/export.h>

namespace cdr {

CDR_OPTIONS_EXPORT f64 FlatForward(const f64 time, const f64 time1, const f64 vol1, const f64 time2, const f64 vol2) noexcept;

} // namespace cdr
