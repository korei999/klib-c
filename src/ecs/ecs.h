#pragma once

#include "klib/IAllocator.h"
#include "klib/assert.h"

typedef int ECS_ENTITY;

typedef struct ecs_Component
{
    void* pData; /* pData and pDense share one buffer. */
    int* pDense;
    int size;
    int cap;
    int freeListSize;
    int* pSparse; /* Of ecs_Map::cap capacity and allocated alongside with the ecs_Map. */
    int* pFreeList; /* Same as pSparse. */
} ecs_Component;

typedef struct ecs_Map
{
    k_IAllocator* pAlloc;

    uint8_t* pDense;

    ECS_ENTITY* pSparse;
    int* pFreeList;
    ecs_Component* pSOAComponents;

    int denseStride;
    int size;
    int cap;
    int freeListSize;

    const int* pSizeMap;
    int sizeMapSize;
} ecs_Map;

bool ecs_MapInit(ecs_Map* s, k_IAllocator* pAlloc, int cap, const int* pSizeMap, int sizeMapSize);
void ecs_MapDestroy(ecs_Map* s);
ECS_ENTITY ecs_MapAddEntity(ecs_Map* s);
void ecs_MapRemove(ecs_Map* s, ECS_ENTITY h, int eComp);
void ecs_MapRemoveEntity(ecs_Map* s, ECS_ENTITY h);
bool ecs_MapAdd(ecs_Map* s, ECS_ENTITY h, int eComp, void* pVal); /* Add eComp component to the entity h. */

static inline void* ecs_MapGet(ecs_Map* s, ECS_ENTITY h, int eComp);
static inline void* ecs_MapAt(ecs_Map* s, int denseI, int eComp);

static inline void*
ecs_MapGet(ecs_Map* s, ECS_ENTITY h, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(h >= 0 && h < s->size, "h: {}, size: {}", h, s->size);
    ecs_Component* pComp = &s->pSOAComponents[eComp];
    const int denseI = pComp->pSparse[h];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}

static inline void*
ecs_MapAt(ecs_Map* s, int denseI, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(denseI >= 0 && denseI < s->cap, "denseI: {}, cap: {}", denseI, s->cap);
    ecs_Component* pComp = &s->pSOAComponents[eComp];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}
