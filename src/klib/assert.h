#pragma once

#include "common.h"

void k_assert_die(const char* ntsFile, ssize_t line, const char* ntsFmt, ...);

#define K_ASSERT_ALWAYS(cnd, ...)                                                                                      \
    if (!(cnd))                                                                                                        \
    {                                                                                                                  \
        k_assert_die(__FILE__, __LINE__, "[assertion( '" #cnd "' ) failed]: " __VA_ARGS__);                            \
    }

#ifndef NDEBUG
    #define K_ASSERT(...) K_ASSERT_ALWAYS(__VA_ARGS__)
#else
    #define K_ASSERT(...) (void)0
#endif
