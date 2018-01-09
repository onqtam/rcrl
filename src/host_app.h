#pragma once

#ifdef _WIN32
#define SYMBOL_EXPORT __declspec(dllexport)
#define SYMBOL_IMPORT __declspec(dllimport)
#else
#define SYMBOL_EXPORT __attribute__((visibility("default")))
#define SYMBOL_IMPORT
#endif

#ifdef HOST_APP
#define HOST_API SYMBOL_EXPORT
#else
#define HOST_API SYMBOL_IMPORT
#endif

// can also use WINDOWS_EXPORT_ALL_SYMBOLS in CMake for Windows
// instead of explicitly annotating each symbol in the host app

HOST_API void print();
HOST_API void draw();
