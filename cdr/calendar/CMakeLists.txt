cdr_cpp_library(
    NAME calendar
    HDRS
      "holiday_storage.h"
      "date.h"
      "freq.h"
    SRCS
      "holiday_storage.cc"
      "date.cc"
    PUBLIC
)

cdr_cpp_test(
    NAME test_date
    SRCS
      "holiday_storage_test.cc"
      "date_test.cc"
    DEPS
      cdr::calendar
      GTest::gtest_main
      GTest::gmock
)
