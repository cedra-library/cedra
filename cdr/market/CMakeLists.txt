cdr_cpp_library(
  NAME market
  HDRS
    "context.h"
  SRCS
    "context.cc"
  DEPS
    cdr::types
    cdr::calendar
    cdr::base
  PUBLIC
)
