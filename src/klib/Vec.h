#pragma once

#include "common.h"
#include "IAllocator.h"

#include <assert.h>

typedef struct k_Vec
{
    void* pData;
    ssize_t size;
    ssize_t cap;
} k_Vec;

static inline bool
k_VecInit(k_Vec* s, k_IAllocator* pAlloc, ssize_t cap, ssize_t mSize)
{
    void* pNew = NULL;
    if (cap > 0)
    {
        pNew = k_IAllocatorMalloc(pAlloc, cap * mSize);
        if (!pNew) return false;
    }

    s->pData = pNew;
    s->size = 0;
    s->cap = cap;

    return true;
}

static inline void
k_VecDestroy(k_Vec* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (k_Vec){0};
}

static inline bool
k_VecGrow(k_Vec* s, k_IAllocator* pAlloc, ssize_t newCap, ssize_t mSize)
{
    void* pNew = k_IAllocatorRealloc(pAlloc, s->pData, s->size * mSize, newCap * mSize);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;

    return true;
}

static inline ssize_t
k_VecPush(k_Vec* s, k_IAllocator* pAlloc, ssize_t mSize, const void* pVal)
{
    if (s->size >= s->cap)
    {
        if (!k_VecGrow(s, pAlloc, K_MAX(8, s->cap * 2), mSize))
            return K_NPOS;
    }

    memcpy((uint8_t*)s->pData + s->size*mSize, pVal, mSize);
    return s->size++;
}

static inline ssize_t
k_VecPushMany(k_Vec* s, k_IAllocator* pAlloc, ssize_t mSize, const void* p, ssize_t size)
{
    if (size <= 0) return s->size;
    if (s->size + size > s->cap)
    {
        ssize_t newCap = s->cap * 2;
        if (s->size + size > newCap) newCap = s->size + size;
        if (!k_VecGrow(s, pAlloc, newCap, mSize))
            return K_NPOS;
    }

    memcpy((uint8_t*)s->pData + s->size*mSize, p, size*mSize);
    s->size += size;
    return s->size - size;
}

static inline void*
k_VecPop(k_Vec* s, ssize_t mSize)
{
    assert(s->size > 0);
    return (uint8_t*)s->pData + --s->size*mSize;
}

static inline bool
k_VecShrink(k_Vec* s, k_IAllocator* pAlloc, ssize_t newCap, ssize_t mSize)
{
    void* pNew = k_IAllocatorRealloc(pAlloc, s->pData, s->cap*mSize, newCap*mSize);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;
    if (s->size > s->cap) s->size = s->cap;

    return true;
}

static inline bool
k_VecPopShrink(k_Vec* s, k_IAllocator* pAlloc, ssize_t mSize)
{
    assert(s->size > 0);
    if (s->size <= s->cap >> 2)
    {
        if (!k_VecShrink(s, pAlloc, s->cap >> 1, mSize))
            return false;
    }
    --s->size;
    return true;
}

static inline bool
k_VecSetCap(k_Vec* s, k_IAllocator* pAlloc, ssize_t newCap, ssize_t mSize)
{
    void* pNew = k_IAllocatorRealloc(pAlloc, s->pData, K_MIN(newCap*mSize, s->size*mSize), newCap*mSize);
    if (!pNew) return false;

    s->pData = pNew;
    s->cap = newCap;
    if (s->size > s->cap) s->size = s->cap;

    return true;
}

static inline void*
k_VecGetP(k_Vec* s, ssize_t mSize, ssize_t i)
{
    assert(i >= 0 && i < s->size);
    return (uint8_t*)s->pData + i*mSize;
}

static inline void
k_VecSet(k_Vec* s, ssize_t i, ssize_t mSize, const void* p)
{
    assert(i >= 0 && i < s->size);
    memcpy((uint8_t*)s->pData + i*mSize, p, mSize);
}
