add_subdirectory(types)
add_subdirectory(math)
add_subdirectory(calendar)
add_subdirectory(curve)
add_subdirectory(base)
add_subdirectory(swaps)
add_subdirectory(options)
add_subdirectory(fx)
add_subdirectory(market)
add_subdirectory(model)


cdr_cpp_test(
  NAME ceres_tests
  SRCS
    "ceres_tests.cc"
  DEPS
    Ceres::ceres
    GTest::gtest_main
)
