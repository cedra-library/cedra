#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_MATH_BUILD_DLL
    #define CDR_MATH_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_MATH_BUILD_DLL
    #define CDR_MATH_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_MATH_BUILD_DLL
