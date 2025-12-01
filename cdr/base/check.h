#pragma once

#include <cdr/base/internal/check_impl.h>

#define CDR_CHECK(condition) \
    ((condition)) ? (void)0 : ::cdr::internal::CheckImpl(#condition, __FILE__, __LINE__)
