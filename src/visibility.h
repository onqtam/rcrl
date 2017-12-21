#pragma once

#ifdef _WIN32
#define SYMBOL_EXPORT __declspec(dllexport)
#define SYMBOL_IMPORT __declspec(dllimport)
#else // _WIN32
#define SYMBOL_EXPORT __attribute__((visibility("default")))
#define SYMBOL_IMPORT
#endif // _WIN32

#ifdef CRCL_HOST_APP
#define HOST_API SYMBOL_EXPORT
#else // CRCL_HOST_APP
#define HOST_API SYMBOL_IMPORT
#endif // CRCL_HOST_APP

// can also use WINDOWS_EXPORT_ALL_SYMBOLS in CMake for Windows
// instead of explicitly annotating each symbol in the host app
