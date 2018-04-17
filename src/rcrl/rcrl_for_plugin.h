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
#define RCRL_ONCE_BEGIN static int RCRL_ANONYMOUS(rcrl_anon_) = []() {
#define RCRL_ONCE_END return 0; }();

// for variable definitions with persistence in the vars section
#define RCRL_VAR(alloc_type, final_type, deref, name, ...)                                                                  \
    static RCRL_HANDLE_BRACED_VA_ARGS(final_type)& name = *[]() {                                                           \
        auto& address = rcrl_get_persistence(#name);                                                                        \
        if(address == nullptr) {                                                                                            \
            address = (void*)new RCRL_HANDLE_BRACED_VA_ARGS(alloc_type) __VA_ARGS__;                                        \
            rcrl_add_deleter(address, [](void* ptr) { delete static_cast<RCRL_HANDLE_BRACED_VA_ARGS(alloc_type)*>(ptr); }); \
        }                                                                                                                   \
        return deref static_cast<RCRL_HANDLE_BRACED_VA_ARGS(alloc_type)*>(address);                                         \
    }()

#define RCRL_AUTO_LAMBDA(name, constness, assignment, ...)                                                                  \
    static auto rcrl_##name##_type_returner = []() -> auto {                                                                \
        constness auto temp assignment __VA_ARGS__;                                                                         \
        return temp;                                                                                                        \
    }

// for variable definitions with auto type - using a lambda and decltype of a call
// to it to figure out the type that the initializer expression would have yielded
#define RCRL_VAR_AUTO(name, constness, assignment, ...)                                                                     \
    RCRL_AUTO_LAMBDA(name, constness, assignment, __VA_ARGS__);                                                             \
    RCRL_VAR((constness decltype(rcrl_##name##_type_returner())), (constness decltype(rcrl_##name##_type_returner())),      \
             RCRL_EMPTY(), name, __VA_ARGS__)

#define RCRL_VAR_AUTO_REF(name, constness, assignment, ...)                                                                 \
    RCRL_AUTO_LAMBDA(name, constness, assignment, __VA_ARGS__);                                                             \
    RCRL_VAR((constness decltype(rcrl_##name##_type_returner())), (constness decltype(*rcrl_##name##_type_returner())), *,  \
             name, __VA_ARGS__)

// the symbols for persistence which the host app should export
RCRL_SYMBOL_IMPORT void*& rcrl_get_persistence(const char* var_name);
RCRL_SYMBOL_IMPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*));
