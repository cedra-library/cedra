#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_SWAPS_BUILD_DLL
    #define CDR_SWAPS_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_SWAPS_BUILD_DLL
    #define CDR_SWAPS_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_SWAPS_BUILD_DLL