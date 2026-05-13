cdr_cpp_library(
  NAME options
  HDRS
    "helpers.h"
    "contract.h"
    "volatility.h"
    "internal/export.h"
    "interpolation/concept.h"
    "interpolation/cubic_spline.h"
    "interpolation/quadratic_spline.h"
    "interpolation/lerp.h"
    "interpolation/flat_forward.h"
    "interpolation/sabr.h"
  SRCS
    "contract.cc"
    "interpolation/cubic_spline.cc"
    "interpolation/quadratic_spline.cc"
    "interpolation/lerp.cc"
    "interpolation/flat_forward.cc"
    "interpolation/sabr.cc"
  DEPS
    cdr::calendar
    cdr::base
    cdr::types
    cdr::math
    Ceres::ceres
  PUBLIC
)


cdr_cpp_test(
  NAME volatility_tests
  SRCS
    "volatility_tests.cc"
  DEPS
    cdr::options
    cdr::curve
    cdr::calendar
    cdr::market
    GTest::gtest_main
)

cdr_cpp_test(
  NAME cubic_spline_tests
  SRCS
    "interpolation/cubic_spline_tests.cc"
  DEPS
    cdr::options
    GTest::gtest_main
)

cdr_cpp_test(
  NAME sabr_tests
  SRCS
    "interpolation/sabr_tests.cc"
  DEPS
    cdr::options
    Ceres::ceres
    GTest::gtest_main
)

cdr_cpp_test(
  NAME option_tests
  SRCS
    "options_tests.cc"
  DEPS
    cdr::options
    GTest::gtest_main
)


# cdr_cpp_executable(
#   NAME
#     volatility_benchmark
#   SRCS
#     "volatility_bench.cc"
#   DEPS
#     cdr::options
#     cdr::calendar
#     benchmark::benchmark
#   COPTS
#     "-O3"
#   BENCH
# )
