cdr_cpp_library(
  NAME curve
  HDRS
    "curve.h"
    "interpolation/linear.h"
  SRCS
    "interpolation/linear.cc"
  DEPS
    cdr::calendar
)
