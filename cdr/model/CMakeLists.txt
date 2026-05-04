cdr_cpp_library(
  NAME model
  HDRS
    "model.h"
    "internal/export.h"
  SRCS "model.cc"
  DEPS
    cdr::base
    cdr::types
    cdr::fx
    cdr::calendar
    cdr::curve
    cdr::swaps
    cdr::market
  PUBLIC
)

cdr_cpp_test(
  NAME model_test
  SRCS
    "model_test.cc"
  DEPS
    cdr::model
    GTest::gtest_main
)
