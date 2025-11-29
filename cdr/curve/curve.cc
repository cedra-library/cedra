#include <cdr/curve/curve.h>

namespace cdr {

Curve::CurveEasyInit Curve::StaticInit() {
    return CurveEasyInit{this};
}

void Curve::Clear() {
    points_.clear();
}

void Curve::Insert(DateType when, Percent value) {
    points_[when] = value;
}

} // namespace cdr
