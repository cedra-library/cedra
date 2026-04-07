cdr_cpp_library(
    NAME calendar
    HDRS
      "internal/export.h"
      "holiday_storage.h"
      "date.h"
      "freq.h"
    SRCS
      "holiday_storage.cc"
      "date.cc"
    DEPS
      cdr::base
      cdr::types
    PUBLIC
)

cdr_cpp_test(
    NAME test_date
    SRCS
      "holiday_storage_test.cc"
      "date_test.cc"
    DEPS
      cdr::calendar
      cdr::base
      GTest::gtest_main
      GTest::gmock
)
