#pragma once

#include "IAllocator.h"

#include <assert.h>

typedef struct k_soa_Lane
{
    void* pData;
    ssize_t size;
    ssize_t cap;
} k_soa_Lane;

typedef struct k_soa_Vec
{
    k_IAllocator* pAlloc;
    k_soa_Lane* pLanes;
    const size_t* pSizeMap;
    ssize_t size;
} k_soa_Vec;

static inline bool
k_soa_VecInit(k_soa_Vec* s, k_IAllocator* pAlloc, const size_t* pSizeMap, ssize_t sizeMapSize)
{
    s->pLanes = k_IAllocatorZalloc(pAlloc, sizeof(k_soa_Lane)*sizeMapSize);
    if (!s->pLanes) return false;

    s->pAlloc = pAlloc;
    s->pSizeMap = pSizeMap;
    s->size = sizeMapSize;

    return true;
}

static inline ssize_t
k_soa_VecPush(k_soa_Vec* s, int iLane, const void* pData)
{
    assert(iLane >= 0 && iLane < s->size);
    k_soa_Lane* pLane = s->pLanes + iLane;
    const size_t memberSize = s->pSizeMap[iLane];

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

static inline void
k_soa_VecDestroy(k_soa_Vec* s)
{
    for (ssize_t i = 0; i < s->size; ++i)
    {
        k_soa_Lane* pLane = s->pLanes + i;
        k_IAllocatorFree(s->pAlloc, pLane->pData);
    }

    k_IAllocatorFree(s->pAlloc, s->pLanes);
    *s = (k_soa_Vec){0};
}
