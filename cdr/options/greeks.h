#pragma once

#include <cdr/types/floats.h>

namespace cdr {

struct Greeks {
    f64 price;
    f64 delta;
    f64 vega;
    f64 rho;
};

} // namespace cdr
