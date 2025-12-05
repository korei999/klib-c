#pragma once

#include "Span.h"
#include "Ctx.h"

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
        K_TYPEOF(*a) K_GLUE(_pivot_, __LINE__);                                                                        \
        K_TYPEOF(*a) K_GLUE(_swap_, __LINE__);                                                                         \
        k_sort_quick(                                                                                                  \
            (k_Span) {a, aSize}, sizeof(*a), &K_GLUE(_pivot_, __LINE__), &K_GLUE(_swap_, __LINE__), pfnCmp, pArg       \
        );                                                                                                             \
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
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        uint8_t* p = pData;
        int* pStack = k_ArenaMalloc(pArena, sizeof(int) * (r - l + 1));
        int top = 0;
        pStack[top++] = l;
        pStack[top++] = r;

        while (top > 0)
        {
            r = pStack[--top];
            l = pStack[--top];

            if (l < r)
            {
                if ((r - l + 1) <= 32)
                {
                    k_sort_insertion2(p, memberSize, pSwap, l, r, pfnCmp, pArg);
                }
                else
                {
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

                    pStack[top++] = l;
                    pStack[top++] = j;

                    pStack[top++] = i;
                    pStack[top++] = r;
                }
            }
        }
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
