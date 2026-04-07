#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_OPTIONS_BUILD_DLL
    #define CDR_OPTIONS_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_OPTIONS_BUILD_DLL
    #define CDR_OPTIONS_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_OPTIONS_BUILD_DLL