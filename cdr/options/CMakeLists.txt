cdr_cpp_library(
  NAME options
  HDRS
    "contract.h"
    "volatility.h"
  SRCS
    "contract.cc"
    "volatility.cc"
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
