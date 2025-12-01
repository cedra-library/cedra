#pragma once

#include <cdr/base/internal/check_impl.h>

#define CDR_CHECK(condition) \
    ::cdr::internal::CheckImpl(((condition)),#condition, __FILE__, __LINE__)
