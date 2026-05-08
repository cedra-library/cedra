cdr_cpp_library(
  NAME options
  HDRS
    "helpers.h"
    "contract.h"
    "volatility.h"
    "internal/export.h"
    "interpolation/concept.h"
    "interpolation/quadratic_spline.h"
  SRCS
    "contract.cc"
    "volatility.cc"
    "interpolation/quadratic_spline.cc"
  DEPS
    cdr::calendar
    cdr::base
    cdr::types
    cdr::math
  PUBLIC
)


cdr_cpp_test(
  NAME volatility_tests
  SRCS
    "volatility_tests.cc"
  DEPS
    cdr::options
    GTest::gtest_main
)

cdr_cpp_executable(
  NAME
    volatility_benchmark
  SRCS
    "volatility_bench.cc"
  DEPS
    cdr::options
    cdr::calendar
    benchmark::benchmark
  COPTS
    "-O3"
  BENCH
)