#include <cmath>
#include <cdr/options/interpolation/flat_forward.h>
#include <cdr/options/interpolation/lerp.h>

namespace cdr {

f64 FlatForward(const f64 time, const f64 time1, const f64 vol1, const f64 time2, const f64 vol2) noexcept {
    const f64 w1 = vol1*vol1 * time1;
    const f64 w2 = vol2*vol2 * time2;
    const f64 w = Lerp(time, time1, w1, time2, w2);
    return std::sqrt(w / time);
}


} // namespace cdr
