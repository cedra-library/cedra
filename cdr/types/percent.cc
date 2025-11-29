#include <cdr/types/percent.h>

namespace cdr {

inline namespace literals {

Percent operator""_percents(unsigned long long int_value) {
    return Percent::FromPercentage(static_cast<f64>(int_value));
}

Percent operator""_percents(long double float_value) {
    return Percent::FromPercentage(static_cast<f64>(float_value));
}

} // namespace literals

} // namespace cdr
