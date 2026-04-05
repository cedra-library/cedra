cdr_cpp_library(
  NAME base
  HDRS
    "generator.h"
    "internal/generator_impl.h"
    "check.h"
    "internal/check_impl.h"
    "hardware_interference_size.h"
  SRCS
    "internal/check_impl.cc"
  DEPS
    cdr::types
  PUBLIC
)

cdr_cpp_executable(
  NAME
    generator_benchmark
  SRCS
    "internal/generator_impl_bench.cc"
  DEPS
    cdr::base
    benchmark::benchmark
  COPTS
    "-O3"
  BENCH
)