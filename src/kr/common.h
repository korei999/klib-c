#pragma once

#include <stdbool.h>
#include <stdint.h>

#if __has_include(<sys/types.h>)
    #include <sys/types.h>

    #define KR_TYPEOF __typeof

#elif defined _MSC_VER

    #define KR_TYPEOF __typeof__

#else
#endif

#define KR_GLUE(x, y) KR_GLUE0(x, y)
#define KR_GLUE0(x, y) x##y
#define KR_GLUE1(x, y) KR_GLUE0(x, y)

#define KR_SCOPE_END(...) for (int _adt_defer_ = 0; !_adt_defer_; _adt_defer_ = 1, (__VA_ARGS__))

#define KR_SCOPE_BEGIN_END(begin, end) for (int _adt_defer_ = ((begin), 0); !_adt_defer_; _adt_defer_ = 1, (end))

#define KR_VAR_SCOPE(type, begin, end)                                                                                 \
    for (type begin, *_adt_defer_p_ = NULL; !_adt_defer_p_; _adt_defer_p_ = (void*)0xCafeBabe, (end))

static const int KR_NPOS = -1;
static const uint8_t KR_NPOS8 = (uint8_t)-1;
static const uint16_t KR_NPOS16 = (uint16_t)-1;
static const uint32_t KR_NPOS32 = (uint32_t)-1;
static const uint64_t KR_NPOS64 = (uint64_t)-1;
