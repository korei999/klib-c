#include "ecs.h"

#include "klib/assert.h"
#include "klib/IAllocator.h"

typedef struct DenseEnum
{
    uint8_t dense;
    uint8_t sparse; /* Holds dense index + 1, such that invalid index is 0. */
} DenseEnum;

typedef struct DenseDesc2
{
    int sparseI;
    uint8_t enumsSize;
    DenseEnum pEnums[];
} DenseDesc2;

static bool MapGrow(ecs_Map* s, int newCap);

bool
ecs_MapInit(ecs_Map* s, k_IAllocator* pAlloc, int cap, const int* pSizeMap, int sizeMapSize)
{
    s->pAlloc = pAlloc;
    s->pSizeMap = pSizeMap;
    s->sizeMapSize = sizeMapSize;

    void* pSOA = k_IAllocatorZalloc(pAlloc, sizeMapSize * sizeof(*s->pSOAComponents));
    if (!pSOA) return false;
    s->pSOAComponents = pSOA;

    s->denseStride = sizeof(DenseDesc2) + sizeof(DenseEnum)*sizeMapSize;

    if (!MapGrow(s, cap))
    {
        k_IAllocatorFree(pAlloc, pSOA);
        s->pSOAComponents = NULL;
        return false;
    }

    return true;
}

void
ecs_MapDestroy(ecs_Map* s)
{
    for (ssize_t i = 0; i < s->sizeMapSize; ++i)
        k_IAllocatorFree(s->pAlloc, s->pSOAComponents[i].pData);

    k_IAllocatorFree(s->pAlloc, s->pSOAComponents);
    k_IAllocatorFree(s->pAlloc, s->pDense);
}

static bool
MapGrow(ecs_Map* s, int newCap)
{
    const ssize_t totalCap = newCap * (
        s->denseStride +
        sizeof(*s->pSparse) +
        sizeof(*s->pFreeList) +
        sizeof(*s->pSOAComponents[0].pSparse)*s->sizeMapSize +
        sizeof(*s->pSOAComponents[0].pFreeList)*s->sizeMapSize
    );
    uint8_t* pNew = k_IAllocatorZalloc(s->pAlloc, totalCap);
    if (!pNew) return false;
    void* pOld = s->pDense;

    ssize_t off = 0;

    {
        if (s->pDense) memcpy(pNew, s->pDense, s->denseStride*s->size);
        s->pDense = pNew;
        off += s->denseStride*newCap;
    }

    {
        if (s->pSparse) memcpy(pNew + off, s->pSparse, sizeof(*s->pSparse)*s->cap);
        s->pSparse = (void*)(pNew + off);

        for (int i = s->cap; i < newCap; ++i)
            s->pSparse[i] = -1;

        off += sizeof(*s->pSparse)*newCap;
    }

    {
        if (s->pFreeList) memcpy(pNew + off, s->pFreeList, sizeof(*s->pFreeList)*s->cap);
        s->pFreeList = (void*)(pNew + off);
        off += sizeof(*s->pFreeList)*newCap;
    }

    /* Populate SOAComponents. */
    for (ssize_t compI = 0; compI < s->sizeMapSize; ++compI)
    {
        if (s->pSOAComponents[compI].pSparse)
            memcpy(pNew + off, s->pSOAComponents[compI].pSparse, sizeof(*s->pSOAComponents[0].pSparse)*s->cap);

        s->pSOAComponents[compI].pSparse = (void*)(pNew + off);
        off += sizeof(*s->pSOAComponents[0].pSparse)*newCap;
        for (int i = s->cap; i < newCap; ++i)
            s->pSOAComponents[compI].pSparse[i] = -1;

        if (s->pSOAComponents[compI].pFreeList)
            memcpy(pNew + off, s->pSOAComponents[compI].pFreeList, sizeof(*s->pSOAComponents[0].pFreeList)*s->cap);

        s->pSOAComponents[compI].pFreeList = (void*)(pNew + off);
        off += sizeof(*s->pSOAComponents[0].pFreeList)*newCap;
    }

    k_IAllocatorFree(s->pAlloc, pOld);
    s->cap = newCap;

    return true;
}

ECS_ENTITY
ecs_MapAddEntity(ecs_Map* s)
{
    if (s->size >= s->cap)
    {
        if (!MapGrow(s, K_MAX(8, s->cap * 2)))
            return -1;
    }

    int i = s->freeListSize > 0 ? s->pFreeList[--s->freeListSize] : s->size;

    s->pSparse[s->size] = i;
    DenseDesc2* pDense = (DenseDesc2*)(s->pDense + s->denseStride*i);
    pDense[i].sparseI = s->size;
    pDense[i].enumsSize = 0;

    ++s->size;

    return i;
}

void
ecs_MapRemove(ecs_Map* s, ECS_ENTITY h, int eComp)
{
    DenseDesc2* pDense = (DenseDesc2*)(s->pDense + s->pSparse[h]*s->denseStride);

    K_ASSERT(pDense->pEnums[eComp].sparse != 0,
        "h: {i}, {i}, off: {sz}, sparse[h]: {i}",
        h, pDense->pEnums[eComp].sparse, ((uint8_t*)pDense - s->pDense)/s->denseStride, s->pSparse[h]
    );

    const int enumDenseI = pDense->pEnums[eComp].sparse - 1;
    pDense->pEnums[enumDenseI].dense = pDense->pEnums[--pDense->enumsSize].dense; /* Swap with last. */
    pDense->pEnums[pDense->pEnums[enumDenseI].dense].sparse = enumDenseI + 1;
    pDense->pEnums[eComp].sparse = 0;

    ecs_Component* pComp = &s->pSOAComponents[eComp];
    const ssize_t compSize = s->pSizeMap[eComp];

    const int denseI = pComp->pSparse[h];
    const int moveSparseI = pComp->pDense[pComp->size - 1];
    const int moveDenseI = pComp->pSparse[moveSparseI];

    K_ASSERT(pComp->pDense[denseI] == h, "sparseI: {i}, h: {i}", pComp->pDense[denseI], h);

    memcpy((uint8_t*)pComp->pData + denseI*compSize, (uint8_t*)pComp->pData + moveDenseI*compSize, compSize);
    pComp->pDense[denseI] = moveSparseI;
    pComp->pSparse[moveSparseI] = denseI;
    pComp->pSparse[h] = -1;
    pComp->pFreeList[pComp->freeListSize++] = denseI;
    --pComp->size;
}

void
ecs_MapRemoveEntity(ecs_Map* s, ECS_ENTITY h)
{
    // DenseDesc* pDense = &s->pDense[s->pSparse[h]];
    DenseDesc2* pDense = (DenseDesc2*)(s->pDense + s->pSparse[h]*s->denseStride);

    while (pDense->enumsSize > 0)
        ecs_MapRemove(s, h, pDense->pEnums[0].dense);

    // DenseDesc* pMoveDense = &s->pDense[s->size - 1];
    DenseDesc2* pMoveDense = (DenseDesc2*)(s->pDense + (s->size - 1)*s->denseStride);

    pDense->sparseI = pMoveDense->sparseI;
    pDense->enumsSize = pMoveDense->enumsSize;
    memcpy(pDense->pEnums, pMoveDense->pEnums, sizeof(*pMoveDense->pEnums)*pMoveDense->enumsSize);

    s->pSparse[pMoveDense->sparseI] = s->pSparse[h];
    s->pSparse[h] = -1;
    s->pFreeList[s->freeListSize++] = s->pSparse[h];
    --s->size;
}

bool
ecs_MapAdd(ecs_Map* s, ECS_ENTITY h, int eComp, void* pVal)
{
    // DenseDesc* pDense = &s->pDense[s->pSparse[h]];
    DenseDesc2* pDense = (DenseDesc2*)(s->pDense + s->pSparse[h]*s->denseStride);

    K_ASSERT(pDense->pEnums[eComp].sparse == 0, "");
    pDense->pEnums[pDense->enumsSize].dense = eComp;
    pDense->pEnums[eComp].sparse = ++pDense->enumsSize; /* Sparse holds dense idx + 1. */

    /* Now we need to add this entity to the SOAComponents[eComp] map. */
    ecs_Component* pComp = &s->pSOAComponents[eComp];
    const ssize_t compSize = s->pSizeMap[eComp];

    if (pComp->size >= pComp->cap)
    {
        const int newCap = K_MAX(8, pComp->cap * 2);
        uint8_t* pNew = k_IAllocatorZalloc(s->pAlloc, newCap*(sizeof(*pComp->pDense) + compSize));
        if (!pNew) return false;
        if (pComp->pData)
        {
            memcpy(pNew, pComp->pData, compSize*pComp->size);
            memcpy(pNew + compSize*newCap, pComp->pDense, sizeof(*pComp->pDense)*pComp->size);
        }
        k_IAllocatorFree(s->pAlloc, pComp->pData);
        pComp->pData = pNew;
        pComp->pDense = (void*)(pNew + compSize*newCap);
        pComp->cap = newCap;
    }

    const int compDenseI = pComp->freeListSize > 0 ? pComp->pFreeList[--pComp->freeListSize] : pComp->size;
    pComp->pSparse[h] = compDenseI;
    pComp->pDense[compDenseI] = h;

    if (pVal) memcpy((uint8_t*)pComp->pData + compDenseI*compSize, pVal, compSize);
    else memset((uint8_t*)pComp->pData + compDenseI*compSize, 0, compSize);

    ++pComp->size;

    return true;
}
