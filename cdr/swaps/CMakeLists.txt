cdr_cpp_library(
    NAME swaps
    HDRS "irs.h"
    SRCS "irs.cc"
    DEPS
      cdr::types
      cdr::calendar
      cdr::generator
    PUBLIC
)

cdr_cpp_test(
    NAME swaps_test
    SRCS
      "swaps_tests.cc"
    DEPS
        cdr::swaps
        GTest::gtest_main
)
