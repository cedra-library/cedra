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
    cdr::fx
  PUBLIC
)

cdr_cpp_test(
  NAME market_context_tests
  SRCS
    "context_tests.cc"
  DEPS
    cdr::market
    GTest::gtest_main
)
