#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_FX_BUILD_DLL
    #define CDR_FX_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_FX_BUILD_DLL
    #define CDR_FX_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_FX_BUILD_DLL