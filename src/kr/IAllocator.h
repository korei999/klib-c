#pragma once

#include "common.h"

#include <string.h>

typedef struct
{
    void* (*malloc)(void* pSelf, ssize_t nBytes);
    void* (*zalloc)(void* pSelf, ssize_t nBytes);
    void* (*realloc)(void* pSelf, void* p, ssize_t nBytesOld, ssize_t nBytesNew);
    void (*free)(void* pSelf, void* p);
} krIAllocatorVTable;

typedef struct
{
    const krIAllocatorVTable* pVTable;
} krIAllocator;

static inline void*
krIAllocatorMalloc(void* pSelf, ssize_t nBytes)
{
    return ((krIAllocator*)pSelf)->pVTable->malloc(pSelf, nBytes);
}

static inline void*
krIAllocatorZalloc(void* pSelf, ssize_t nBytes)
{
    return ((krIAllocator*)pSelf)->pVTable->zalloc(pSelf, nBytes);
}

static inline void*
krIAllocatorRealloc(void* pSelf, void* p, ssize_t nBytesOld, ssize_t nBytesNew)
{
    return ((krIAllocator*)pSelf)->pVTable->realloc(pSelf, p, nBytesOld, nBytesNew);
}

static inline void
krIAllocatorFree(void* pSelf, void* p)
{
    ((krIAllocator*)pSelf)->pVTable->free(pSelf, p);
}

static inline void*
krIAllocatorAlloc(void* pSelf, void* p, ssize_t size)
{
    void* ret = ((krIAllocator*)pSelf)->pVTable->malloc(pSelf, size);
    memcpy(ret, p, size);
    return ret;
}

#define KR_ALIGN_UP(x, to) ((x) + to - 1) & (~(to - 1))
#define KR_ALIGN_DOWN(x, to) (x & ~(to - 1))
#define KR_ALIGN_UP8(x) KR_ALIGN_UP(x, 8)
#define KR_ALIGN_DOWN8(x) KR_ALIGN_DOWN(x, 8)

#define KR_IALLOC(pAlloc, type, ...) (type*)krIAllocatorAlloc(pAlloc, &(type) {__VA_ARGS__}, sizeof(type))
#define KR_IMALLOC(pAlloc, type, count) (type*)krIAllocatorMalloc(pAlloc, sizeof(type) * count)
#define KR_IZALLOC(pAlloc, type, count) (type*)krIAllocatorZalloc(pAlloc, sizeof(type) * count)

static inline ssize_t
krNextPowerofTwo64(ssize_t x)
{
    --x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;

    return ++x;
}
