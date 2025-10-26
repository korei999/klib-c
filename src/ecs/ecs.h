#pragma once

#include "klib/IAllocator.h"
#include "klib/assert.h"

typedef int ENTITY_HANDLE;
static const ENTITY_HANDLE ENTITY_HANDLE_INVALID = -1;

typedef struct SOAComponent
{
    void* pData; /* pData and pDense are both allocated with one calloc(). */
    int* pDense;
    int size;
    int cap;
    int freeListSize;
    int* pSparse; /* Has ComponentMap::cap capacity and allocated alongside with the ComponentMap. */
    int* pFreeList; /* Allocated just like sparse. */
} SOAComponent;

typedef struct ComponentMap
{
    k_IAllocator* pAlloc;

    uint8_t* pDense;

    ENTITY_HANDLE* pSparse;
    int* pFreeList;
    SOAComponent* pSOAComponents;

    int denseStride;
    int size;
    int cap;
    int freeListSize;

    const int* pSizeMap;
    int sizeMapSize;
} ComponentMap;


bool ComponentMapInit(ComponentMap* s, k_IAllocator* pAlloc, int cap, const int* pSizeMap, int sizeMapSize);
void ComponentMapDestroy(ComponentMap* s);
ENTITY_HANDLE ComponentMapAddEntity(ComponentMap* s);
void ComponentMapRemove(ComponentMap* s, ENTITY_HANDLE h, int eComp);
void ComponentMapRemoveEntity(ComponentMap* s, ENTITY_HANDLE h);
bool ComponentMapAdd(ComponentMap* s, ENTITY_HANDLE h, int eComp, void* pVal); /* Add eComp component to the entity h. */

static inline void* ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, int eComp);
static inline void* ComponentMapAt(ComponentMap* s, int denseI, int eComp);

static inline void*
ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(h >= 0 && h < s->size, "h: {}, size: {}", h, s->size);
    SOAComponent* pComp = &s->pSOAComponents[eComp];
    const int denseI = pComp->pSparse[h];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}

static inline void*
ComponentMapAt(ComponentMap* s, int denseI, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(denseI >= 0 && denseI < s->cap, "denseI: {}, cap: {}", denseI, s->cap);
    SOAComponent* pComp = &s->pSOAComponents[eComp];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}
