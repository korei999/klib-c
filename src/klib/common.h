#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined _WIN32
    #include <sys/types.h>

    #define K_TYPEOF __typeof__

    typedef long long ssize_t;

#elif defined __unix__
    #include <sys/types.h>

    #define K_TYPEOF __typeof

    #if defined _MSC_VER
        typedef long long ssize_t;
    #endif

#endif

static const int K_NPOS = -1;
static const uint8_t K_NPOS8 = (uint8_t)-1;
static const uint16_t K_NPOS16 = (uint16_t)-1;
static const uint32_t K_NPOS32 = (uint32_t)-1;
static const uint64_t K_NPOS64 = (uint64_t)-1;

#define K_GLUE(x, y) K_GLUE0(x, y)
#define K_GLUE0(x, y) x##y
#define K_GLUE1(x, y) K_GLUE0(x, y)

#define K_SCOPE_END(...) for (int _adt_defer_ = 0; !_adt_defer_; _adt_defer_ = 1, (__VA_ARGS__))

#define K_SCOPE_BEGIN_END(begin, end) for (int _adt_defer_ = ((begin), 0); !_adt_defer_; _adt_defer_ = 1, (end))

#define K_VAR_SCOPE(type, init, destroy)                                                                               \
    for (type init, *_adt_defer_p_ = NULL; !_adt_defer_p_; _adt_defer_p_ = (void*)0xCafeBabeLLU, (destroy))

#define K_ASIZE(a) ((ssize_t)sizeof(a) / (ssize_t)sizeof((a)[0]))

#define K_MIN(x, y) ((x) < (y) ? (x) : (y))
#define K_MAX(x, y) ((x) > (y) ? (x) : (y))
#define K_SWAP(x, y)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        K_TYPEOF(x) _k_tmp_ = x;                                                                                       \
        x = y;                                                                                                         \
        y = _k_tmp_;                                                                                                   \
    } while (0)\

#ifdef _MSC_VER

    #define K_UNUSED
    #define K_NO_UB
    #define K_NO_DISCARD _Check_return_
    #define K_ALWAYS_INLINE __forceinline

#elif defined __clang__ || defined __GNUC__

    #define K_UNUSED __attribute__((unused))
    #define K_NO_UB __attribute__((no_sanitize("undefined")))
    #define K_NO_DISCARD __attribute__((warn_unused_result))
    #define K_ALWAYS_INLINE __attribute__((always_inline)) inline

#else
#endif
