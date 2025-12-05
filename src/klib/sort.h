#pragma once

#include "Span.h"

static inline ssize_t k_sort_median3(ssize_t x, ssize_t y, ssize_t z);
static inline void k_sort_quick(
    k_Span sp,
    ssize_t memberSize,
    void* pPivot, /* Of memberSize size. */
    void* pSwap, /* Of memberSize size. */
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);
static inline void k_sort_quick2(
    void* pData,
    ssize_t memberSize,
    void* pPivot, /* Of memberSize size. */
    void* pSwap, /* Of memberSize size. */
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);
static inline void k_sort_insertion(
    k_Span sp,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size. */
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);
static inline void k_sort_insertion2(
    void* pData,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size. */
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
);

#define K_SORT_QUICK(a, aSize, pfnCmp, pArg)                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        K_TYPEOF(*a) _pivot_;                                                                                          \
        K_TYPEOF(*a) _swap_;                                                                                           \
        k_sort_quick((k_Span) {a, aSize}, sizeof(*a), &_pivot_, &_swap_, pfnCmp, pArg);                                \
    } while (0)

static inline ssize_t
k_sort_median3(ssize_t x, ssize_t y, ssize_t z)
{
    if ((x < y && y < z) || (z < y && y < x)) return y;
    else if ((y < x && x < z) || (z < x && x < y)) return x;
    else return z;
}

static inline void
k_sort_quick(
    k_Span sp,
    ssize_t memberSize,
    void* pPivot,
    void* pSwap,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    if (sp.size <= 1) return;
    k_sort_quick2(sp.pData, memberSize, pPivot, pSwap, 0, sp.size - 1, pfnCmp, pArg);
}

static inline void
k_sort_quick2(
    void* pData,
    ssize_t memberSize,
    void* pPivot,
    void* pSwap,
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    uint8_t* p = pData;

    if (l < r)
    {
        if ((r - l + 1) <= 32)
        {
            k_sort_insertion2(p, memberSize, pSwap, l, r, pfnCmp, pArg);
            return;
        }

        ssize_t pivotI = k_sort_median3(l, (l + r) / 2, r);
        memcpy(pPivot, p + pivotI*memberSize, memberSize);
        ssize_t i = l, j = r;

        while (i <= j)
        {
            while (pfnCmp(p + i*memberSize, pPivot, pArg) < 0) ++i;
            while (pfnCmp(p + j*memberSize, pPivot, pArg) > 0) --j;

            if (i <= j)
            {
                /* Swap. */
                memcpy(pSwap, p + i*memberSize, memberSize);
                memcpy(p + i*memberSize, p + j*memberSize, memberSize);
                memcpy(p + j*memberSize, pSwap, memberSize);
                ++i, --j;
            }
        }

        k_sort_quick2(p, memberSize, pPivot, pSwap, l, j, pfnCmp, pArg);
        k_sort_quick2(p, memberSize, pPivot, pSwap, i, r, pfnCmp, pArg);
    }
}

static inline void
k_sort_insertion(
    k_Span sp,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size. */
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    if (sp.size <= 1) return;
    k_sort_insertion2(sp.pData, memberSize, pSwap, 0, sp.size - 1, pfnCmp, pArg);
}

static inline void
k_sort_insertion2(
    void* pData,
    ssize_t memberSize,
    void* pSwap, /* Of memberSize size. */
    ssize_t l,
    ssize_t r,
    ssize_t (*pfnCmp)(const void* pL, const void* pR, void* pArg),
    void* pArg
)
{
    uint8_t* p = pData;

    for (ssize_t i = l + 1; i < r + 1; ++i)
    {
        memcpy(pSwap, p + i*memberSize, memberSize);
        ssize_t j = i;
        for (; j > l && pfnCmp(p + (j - 1)*memberSize, pSwap, pArg) > 0; --j)
            memcpy(p + j*memberSize, p + (j - 1)*memberSize, memberSize);

        memcpy(p + j*memberSize, pSwap, memberSize);
    }
}
