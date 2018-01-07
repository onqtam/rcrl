#pragma once

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
    RCRL_HANDLE_BRACED_VA_ARGS(type)* name##_ptr = []() {                                                                   \
        auto& address = rcrl_get_persistence(#name);                                                                        \
        if(address == nullptr) {                                                                                            \
            address = new RCRL_HANDLE_BRACED_VA_ARGS(type) __VA_ARGS__;                                                     \
            rcrl_add_deleter(address, [](void* ptr) { delete static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(ptr); });       \
        }                                                                                                                   \
        return static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(address);                                                     \
    }();                                                                                                                    \
    RCRL_HANDLE_BRACED_VA_ARGS(type)& name = *name##_ptr

SYMBOL_IMPORT void*& rcrl_get_persistence(const char*);
SYMBOL_IMPORT void   rcrl_add_deleter(void*, void (*)(void*));
