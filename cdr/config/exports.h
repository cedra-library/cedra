#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #define CDR_HELPER_DLL_IMPORT __declspec(dllimport)
  #define CDR_HELPER_DLL_EXPORT __declspec(dllexport)
#else
  #if __GNUC__ >= 4
    #define CDR_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define CDR_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
  #else
    #define CDR_HELPER_DLL_IMPORT
    #define CDR_HELPER_DLL_EXPORT
  #endif
#endif
