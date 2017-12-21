#pragma once

#ifdef _WIN32
#define SYMBOL_EXPORT __declspec(dllexport)
#define SYMBOL_IMPORT __declspec(dllimport)
#else // _WIN32
#define SYMBOL_EXPORT __attribute__((visibility("default")))
#define SYMBOL_IMPORT
#endif // _WIN32

#ifdef HOST_APP
#define HOST_API SYMBOL_EXPORT
#else // HOST_APP
#define HOST_API SYMBOL_IMPORT
#endif // HOST_APP
