#include "IAllocator.h"

#include <assert.h>

#ifndef K_NAME
    #error "K_NAME is not defined"
#endif

#ifndef K_SIZE_MAP
    #error "K_SIZE_MAP is not defined"
#endif

#ifndef K_DECL_MOD
    #define K_DECL_MOD static inline
#endif

#if !defined K_GEN_DECLS && !defined K_GEN_CODE
    #define K_GEN_DECLS
    #define K_GEN_CODE
#endif

#define K_METHOD(M) K_GLUE(K_NAME, M)
#define K_LANE K_GLUE(K_NAME, Lane)

#ifdef K_GEN_DECLS

typedef struct K_LANE
{
    void* pData;
    ssize_t size;
    ssize_t cap;
} K_LANE;

typedef struct K_NAME
{
    k_IAllocator* pAlloc;
    K_LANE aLanes[K_ASIZE(K_SIZE_MAP)];
} K_NAME;

K_DECL_MOD bool K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, ssize_t cap);
K_DECL_MOD ssize_t K_METHOD(Push)(K_NAME* s, int iLane, const void* pData);
K_DECL_MOD void K_METHOD(Destroy)(K_NAME* s);

#endif /* K_GEN_DECLS */

#ifdef K_GEN_CODE

K_DECL_MOD bool
K_METHOD(Init)(K_NAME* s, k_IAllocator* pAlloc, ssize_t cap)
{
    *s = (K_NAME){0};

    for (ssize_t i = 0; i < K_ASIZE(K_SIZE_MAP); ++i)
    {
        void* pNewLane = k_IAllocatorMalloc(pAlloc, sizeof(K_SIZE_MAP[i]) * cap);
        if (!pNewLane) goto fail;

        K_LANE* pLane = s->aLanes + i;
        pLane->pData = pNewLane;
        pLane->cap = cap;
    }

    s->pAlloc = pAlloc;
    return true;

fail:
    for (ssize_t i = 0; i < K_ASIZE(K_SIZE_MAP); ++i)
    {
        K_LANE* pLane = s->aLanes + i;
        k_IAllocatorFree(pAlloc, pLane->pData);
        pLane->pData = NULL;
        pLane->size = pLane->cap = 0;
    }
    return false;
}

K_DECL_MOD ssize_t
K_METHOD(Push)(K_NAME* s, int iLane, const void* pData)
{
    assert(iLane >= 0 && iLane < K_ASIZE(K_SIZE_MAP));
    K_LANE* pLane = s->aLanes + iLane;
    const size_t memberSize = K_SIZE_MAP[iLane];

    if (pLane->size >= pLane->cap)
    {
        const ssize_t newCap = K_MAX(2, pLane->size*2);
        void* pNew = k_IAllocatorRealloc(s->pAlloc, pLane->pData, pLane->cap*memberSize, newCap*memberSize);
        if (!pNew) return -1;
        pLane->pData = pNew;
        pLane->cap = newCap;
    }

    memcpy((uint8_t*)pLane->pData + pLane->size*memberSize, pData, memberSize);
    ++pLane->size;
    return 0;
}

K_DECL_MOD void
K_METHOD(Destroy)(K_NAME* s)
{
    for (ssize_t i = 0; i < K_ASIZE(K_SIZE_MAP); ++i)
    {
        K_LANE* pLane = s->aLanes + i;
        k_IAllocatorFree(s->pAlloc, pLane->pData);
    }

    *s = (K_NAME){0};
}

#endif /* K_GEN_CODE */

#undef K_LANE
#undef K_METHOD

#undef K_NAME
#undef K_SIZE_MAP
#undef K_DECL_MOD

#undef K_GEN_DECLS
#undef K_GEN_CODE
