cdr_cpp_library(
  NAME math
  HDRS
    "newton_raphson/newton_raphson.h"
  SRCS
    "newton_raphson/newton_raphson.cc"
  DEPS
    cdr::base
    cdr::types
)
