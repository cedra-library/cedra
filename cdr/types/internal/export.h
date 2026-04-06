#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_TYPES_BUILD_DLL
    #define CDR_TYPES_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_TYPES_BUILD_DLL
    #define CDR_TYPES_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_TYPES_BUILD_DLL