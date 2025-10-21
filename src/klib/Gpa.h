#include "IAllocator.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    k_IAllocator base;
} k_Gpa;

K_NO_DISCARD static inline void* k_GpaMalloc(void* s, ssize_t nBytes);
K_NO_DISCARD static inline void* k_GpaZalloc(void* s, ssize_t nBytes);
K_NO_DISCARD static inline void* k_GpaRealloc(void* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew);
static inline void k_GpaFree(void* s, void* p);
K_NO_DISCARD static inline void* k_GpaAlloc(void* s, void* p, ssize_t size);
static inline void k_GpaInit(k_Gpa* s);
static inline k_Gpa k_GpaCreate(void);
K_NO_DISCARD static inline k_Gpa* k_GpaInst(void);

#define K_GPA_ALLOC(type, ...) (type*)k_GpaAlloc(NULL, &(type) {__VA_ARGS__}, sizeof(type))

static const k_IAllocatorVTable k_s_vtGpa = {
    .malloc = k_GpaMalloc,
    .zalloc = k_GpaZalloc,
    .realloc = k_GpaRealloc,
    .free = k_GpaFree,
};

static k_Gpa k_s_gpa = {.base = {.pVTable = &k_s_vtGpa}};

static inline void*
k_GpaMalloc(void* s, ssize_t nBytes)
{
    (void)s;
    return malloc(nBytes);
}

static inline void*
k_GpaZalloc(void* s, ssize_t nBytes)
{
    (void)s;
    return calloc(1, nBytes);
}

static inline void*
k_GpaRealloc(void* s, void* p, ssize_t nBytesOld, ssize_t nBytesNew)
{
    (void)s;
    (void)nBytesOld;
    return realloc(p, nBytesNew);
}

static inline void
k_GpaFree(void* s, void* p)
{
    (void)s;
    free(p);
}

static inline void*
k_GpaAlloc(void* s, void* p, ssize_t size)
{
    (void)s;
    void* ret = malloc(size);
    memcpy(ret, p, size);
    return ret;
}

static inline void
k_GpaInit(k_Gpa* s)
{
    s->base.pVTable = &k_s_vtGpa;
}

static inline k_Gpa
k_GpaCreate(void)
{
    k_Gpa ret;
    k_GpaInit(&ret);
    return ret;
}

static inline k_Gpa*
k_GpaInst(void)
{
    return &k_s_gpa;
}
