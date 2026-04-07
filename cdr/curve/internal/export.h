#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_CURVE_BUILD_DLL
    #define CDR_CURVE_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_CURVE_BUILD_DLL
    #define CDR_CURVE_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_CURVE_BUILD_DLL