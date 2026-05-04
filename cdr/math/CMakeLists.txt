cdr_cpp_library(
  NAME math
  HDRS
    "newton_raphson/newton_raphson.h"
    "distributions/normal.h"
    "internal/export.h"
  DEPS
    cdr::base
    cdr::types
  PUBLIC
)

cdr_cpp_test(
  NAME normal_tests
  SRCS
    "distributions/normal_tests.cc"
  DEPS
    cdr::math
    GTest::gtest_main
)
