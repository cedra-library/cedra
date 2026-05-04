#pragma once

#include <cdr/config/exports.h>

#ifdef CDR_MARKET_BUILD_DLL
    #define CDR_MARKET_EXPORT CDR_HELPER_DLL_EXPORT
#else // CDR_MARKET_BUILD_DLL
    #define CDR_MARKET_EXPORT CDR_HELPER_DLL_IMPORT
#endif // CDR_MARKET_BUILD_DLL