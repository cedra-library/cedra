#include <cmath>
#include <cdr/options/interpolation/lerp.h>

namespace cdr {

f64 Lerp(const f64 x, const f64 x1, const f64 y1, const f64 x2, const f64 y2) noexcept {
    if (std::abs(x2 - x1) < 1e-9)  {
        return y1;
    }
    const f64 t = (x - x1) / (x2 - x1);
    return y1 + t * (y2 - y1);
}

} // namespace cdr
