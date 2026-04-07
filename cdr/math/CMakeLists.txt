cdr_cpp_library(
  NAME math
  HDRS
    "newton_raphson/newton_raphson.h"
    "internal/export.h"
  SRCS
    "newton_raphson/newton_raphson.cc"
  DEPS
    cdr::base
    cdr::types
  PUBLIC # TO MAKE VISIBLE AFTER INSTALLING
)
