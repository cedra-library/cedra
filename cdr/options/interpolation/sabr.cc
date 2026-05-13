#include <cdr/options/interpolation/sabr.h>

namespace cdr {

Expect<void, Error> SABRInterolator::InitState(void* coefs_ptr, std::span<const f64> xs,
                                               std::span<const f64> ys) noexcept {
    return Ok();
}

[[nodiscard]] Expect<f64, Error> SABRInterolator::Evaluate(const void* coefs_ptr, std::span<const f64> xs,
                                                           f64 x) noexcept {
    return Ok(0.0);
}

}  // namespace cdr
