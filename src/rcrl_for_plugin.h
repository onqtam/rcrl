#pragma once

#include <initializer_list>

#define RCRL_EMPTY()

#define RCRL_CAT_IMPL(s1, s2) s1##s2
#define RCRL_CAT(s1, s2) RCRL_CAT_IMPL(s1, s2)
#define RCRL_ANONYMOUS(x) RCRL_CAT(x, __COUNTER__)

#define RCRL_STRIP_PARENS(x) x
#define RCRL_EXPAND_VA_ARGS(...) __VA_ARGS__
#define RCRL_HANDLE_BRACED_VA_ARGS(expr) RCRL_STRIP_PARENS(RCRL_EXPAND_VA_ARGS expr)

#ifdef _WIN32
#define SYMBOL_IMPORT __declspec(dllimport)
#else
#define SYMBOL_IMPORT
#endif

// for once
#define RCRL_ONCE_BEGIN int RCRL_ANONYMOUS(rcrl_anon_) = []() {
#define RCRL_ONCE_END return 0; }();

// for vars
#define RCRL_VAR(type, name, ...)                                                                                           \
    RCRL_HANDLE_BRACED_VA_ARGS(type)* rcrl_##name##_ptr = []() {                                                            \
        auto& address = rcrl_get_persistence(#name);                                                                        \
        if(address == nullptr) {                                                                                            \
            address = (void*)new RCRL_HANDLE_BRACED_VA_ARGS(type) __VA_ARGS__;                                              \
            rcrl_add_deleter(address, [](void* ptr) { delete static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(ptr); });       \
        }                                                                                                                   \
        return static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(address);                                                     \
    }();                                                                                                                    \
    RCRL_HANDLE_BRACED_VA_ARGS(type)& name = *rcrl_##name##_ptr

// for vars with auto type
#define RCRL_VAR_AUTO(name, constness, assignment, ...)                                                                     \
    auto rcrl_##name##_type_returner = []() -> auto {                                                                       \
        constness auto temp assignment __VA_ARGS__;                                                                         \
        return temp;                                                                                                        \
    };                                                                                                                      \
    RCRL_VAR((constness decltype(rcrl_##name##_type_returner())), name, __VA_ARGS__)

// the symbols for persistence which the host app should export
SYMBOL_IMPORT void*& rcrl_get_persistence(const char* var_name);
SYMBOL_IMPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*));
