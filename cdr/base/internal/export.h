#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_BASE_BUILD_DLL
    #define CDR_BASE_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_BASE_BUILD_DLL
    #define CDR_BASE_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_BASE_BUILD_DLL