#include "IAllocator.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    krIAllocator base;
} krLibcAllocator;

static inline void*
krLibcAllocatorMalloc(void* pSelf, ssize_t nBytes)
{
    void* ret = malloc(nBytes);
    if (!ret)
    {
        fprintf(stderr, "malloc\n");
        abort();
    }
    return ret;
};

static inline void*
krLibcAllocatorZalloc(void* pSelf, ssize_t nBytes)
{
    (void)pSelf;
    void* ret = calloc(1, nBytes);
    if (!ret)
    {
        fprintf(stderr, "calloc\n");
        abort();
    }
    return ret;
};

static inline void*
krLibcAllocatorRealloc(void* pSelf, void* p, ssize_t nBytesOld, ssize_t nBytesNew)
{
    (void)pSelf;
    (void)nBytesOld;
    void* ret = realloc(p, nBytesNew);
    if (!ret)
    {
        fprintf(stderr, "realloc\n");
        abort();
    }
    return ret;
};

static inline void
krLibcAllocatorFree(void* pSelf, void* p)
{
    (void)pSelf;
    free(p);
}

static inline void*
krLibcAllocatorAlloc(void* pSelf, void* p, ssize_t size)
{
    (void)pSelf;
    void* ret = malloc(size);
    memcpy(ret, p, size);
    return ret;
}

static const krIAllocatorVTable kr_s_vtLibcAllocator = {
    .malloc = krLibcAllocatorMalloc,
    .zalloc = krLibcAllocatorZalloc,
    .realloc = krLibcAllocatorRealloc,
    .free = krLibcAllocatorFree,
};

static inline void
krLibcAllocatorInit(krLibcAllocator* pSelf)
{
    pSelf->base.pVTable = &kr_s_vtLibcAllocator;
}

static inline krLibcAllocator
krLibcAllocatorCreate()
{
    krLibcAllocator ret;
    krLibcAllocatorInit(&ret);
    return ret;
};

static inline krLibcAllocator*
krLibcAllocatorInst()
{
    static krLibcAllocator s_inst;
    static bool s_bInitialized = false;
    if (!s_bInitialized)
    {
        s_inst = krLibcAllocatorCreate();
        s_bInitialized = true;
    }
    return &s_inst;
}

#define KR_LIBC_ALLOC(type, ...) (type*)krLibcAllocatorAlloc(NULL, &(type) {__VA_ARGS__}, sizeof(type))
