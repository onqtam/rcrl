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
#else // _WIN32
#define SYMBOL_IMPORT
#endif // _WIN32

#define RCRL_ONCE_BEGIN int RCRL_ANONYMOUS(rcrl_anon_) = []() {
#define RCRL_ONCE_END return 0; }();

#define RCRL_VAR(type, name, ...)                                                               \
    RCRL_HANDLE_BRACED_VA_ARGS(type)* name##_ptr = []() {                                       \
        auto& address = rcrl_persistence[#name];                                                \
        if(address == nullptr) {                                                                \
            address = new RCRL_HANDLE_BRACED_VA_ARGS(type) __VA_ARGS__;                         \
            rcrl_deleters.push_back({address, rcrl_deleter<RCRL_HANDLE_BRACED_VA_ARGS(type)>}); \
        }                                                                                       \
        return static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(address);                         \
    }();                                                                                        \
    RCRL_HANDLE_BRACED_VA_ARGS(type)& name = *name##_ptr

#include <map>
#include <vector>
#include <string>

template <typename T>
void rcrl_deleter(void* ptr) {
    delete static_cast<T*>(ptr);
}

extern SYMBOL_IMPORT std::map<std::string, void*> rcrl_persistence;
extern SYMBOL_IMPORT std::vector<std::pair<void*, void (*)(void*)>> rcrl_deleters;
