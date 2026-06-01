#pragma once

#include "klib/IAllocator.h"
#include "klib/assert.h"
#include "klib/print.h"

typedef struct
{
    int id; /* Sparse index. */
    uint32_t gen; /* Generation. */
} ecs_Entity;

typedef struct
{
    void* pData; /* pData and pDense share one buffer. */
    int* pDense;
    int size;
    int cap;
    int* pSparse; /* Of ecs_Map::cap capacity and allocated alongside with ecs_Map. */
} ecs_Component;

typedef struct
{
    k_IAllocator* pAlloc;

    uint8_t* pDense;

    int* pSparse; /* Entity handle is a sparse index to ecs_Map. */
    uint32_t* pGenerations;
    int* pFreeList; /* Add removed sparse indices to this freeList. */
    ecs_Component* pSOAComponents;

    int sparseSize;
    int denseStride;
    int size;
    int cap;
    int freeListSize;

    const int* pSizeMap;
    int sizeMapSize;
} ecs_Map;

bool ecs_MapInit(ecs_Map* s, k_IAllocator* pAlloc, int cap, const int* pSizeMap, int sizeMapSize);
void ecs_MapDestroy(ecs_Map* s);
ecs_Entity ecs_MapAddEntity(ecs_Map* s);
void ecs_MapRemove(ecs_Map* s, ecs_Entity h, int eComp);
void ecs_MapRemoveEntity(ecs_Map* s, ecs_Entity h);
bool ecs_MapAdd(ecs_Map* s, ecs_Entity h, int eComp, void* pVal); /* Add eComp component to the entity h. */
bool ecs_MapHas(ecs_Map* s, ecs_Entity h, int eComp);
ssize_t ecs_EntityFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* p);

static inline void* ecs_MapGet(ecs_Map* s, ecs_Entity h, int eComp);
static inline void* ecs_MapAt(ecs_Map* s, int denseI, int eComp);

static inline void*
ecs_MapGet(ecs_Map* s, ecs_Entity h, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(h.id >= 0 && h.id < s->size, "h: {i}, size: {i}", h, s->size);
    ecs_Component* pComp = &s->pSOAComponents[eComp];
    const int denseI = pComp->pSparse[h.id];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}

static inline void*
ecs_MapAt(ecs_Map* s, int denseI, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(denseI >= 0 && denseI < s->cap, "denseI: {i}, cap: {i}", denseI, s->cap);
    ecs_Component* pComp = &s->pSOAComponents[eComp];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}
