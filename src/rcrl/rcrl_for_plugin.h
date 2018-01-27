#pragma once

#define RCRL_EMPTY()

#define RCRL_CAT_IMPL(s1, s2) s1##s2
#define RCRL_CAT(s1, s2) RCRL_CAT_IMPL(s1, s2)
#define RCRL_ANONYMOUS(x) RCRL_CAT(x, __COUNTER__)

#define RCRL_STRIP_PARENS(x) x
#define RCRL_EXPAND_VA_ARGS(...) __VA_ARGS__
// used for unpacking expressions with commas from a single macro argument - enclosed by parens
#define RCRL_HANDLE_BRACED_VA_ARGS(expr) RCRL_STRIP_PARENS(RCRL_EXPAND_VA_ARGS expr)

#ifdef _WIN32
#define RCRL_SYMBOL_IMPORT __declspec(dllimport)
#else
#define RCRL_SYMBOL_IMPORT
#endif

// for statements inside of a once section
#define RCRL_ONCE_BEGIN int RCRL_ANONYMOUS(rcrl_anon_) = []() {
#define RCRL_ONCE_END return 0; }();

// for variable definitions with persistence in the vars section
#define RCRL_VAR(type, final_type, deref, name, ...)                                                                        \
    RCRL_HANDLE_BRACED_VA_ARGS(final_type)& name = *[]() {                                                                  \
        auto& address = rcrl_get_persistence(#name);                                                                        \
        if(address == nullptr) {                                                                                            \
            address = (void*)new RCRL_HANDLE_BRACED_VA_ARGS(type) __VA_ARGS__;                                              \
            rcrl_add_deleter(address, [](void* ptr) { delete static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(ptr); });       \
        }                                                                                                                   \
        return deref static_cast<RCRL_HANDLE_BRACED_VA_ARGS(type)*>(address);                                               \
    }()

// for variable definitions with auto type - using a lambda and decltype of a call
// to it to figure out the type that the initializer expression would have yelded
#define RCRL_VAR_AUTO(name, constness, assignment, ...)                                                                     \
    auto rcrl_##name##_type_returner = []() -> auto {                                                                       \
        constness auto temp assignment __VA_ARGS__;                                                                         \
        return temp;                                                                                                        \
    };                                                                                                                      \
    RCRL_VAR((constness decltype(rcrl_##name##_type_returner())), (constness decltype(rcrl_##name##_type_returner())),      \
             RCRL_EMPTY(), name, __VA_ARGS__)

// the symbols for persistence which the host app should export
RCRL_SYMBOL_IMPORT void*& rcrl_get_persistence(const char* var_name);
RCRL_SYMBOL_IMPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*));
