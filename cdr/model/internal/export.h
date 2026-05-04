#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_MODEL_BUILD_DLL
    #define CDR_MODEL_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_MODEL_BUILD_DLL
    #define CDR_MODEL_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_MODEL_BUILD_DLL