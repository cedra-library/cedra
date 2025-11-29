cdr_cpp_library(
  NAME curve
  HDRS
    "curve.h"
    "interpolation/linear.h"
  SRCS
    "curve.cc"
    "interpolation/linear.cc"
  DEPS
    cdr::calendar
)
