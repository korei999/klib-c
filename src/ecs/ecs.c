#include "ecs.h"

#include "klib/assert.h"
#include "klib/IAllocator.h"


/* Entity stores info about what components it has in array of DenseEnums.
 * Using uint8_t we have up to 254 (0 for invalid sparse index) components per entity. */
typedef struct DenseEnum
{
    uint8_t dense;
    uint8_t sparse; /* Holds dense index + 1, such that invalid index is 0. */
} DenseEnum;

typedef struct DenseDesc
{
    int sparseI; /* Back to sparse mapping. */
    int enumsSize;
    DenseEnum pEnums[]; /* The capacity of this array must be equal to ecs_Map::sizeMapSize. */
} DenseDesc;

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

    s->denseStride = sizeof(DenseDesc) + K_ALIGN_UP(sizeof(DenseEnum)*sizeMapSize, _Alignof(DenseDesc));

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
        sizeof(*s->pSOAComponents[0].pSparse)*s->sizeMapSize
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

    int sparseI = s->freeListSize > 0 ? s->pFreeList[--s->freeListSize] : s->sparseSize++;
    s->pSparse[sparseI] = s->size;
    DenseDesc* pDense = (DenseDesc*)(s->pDense + s->denseStride*s->size);
    pDense->sparseI = sparseI;
    pDense->enumsSize = 0;
    memset(pDense->pEnums, 0, s->sizeMapSize*sizeof(DenseEnum)); /* FIXME: Necessary?. */
    ++s->size;

    return sparseI;
}

void
ecs_MapRemove(ecs_Map* s, ECS_ENTITY h, int eComp)
{
    DenseDesc* pDense = (DenseDesc*)(s->pDense + s->pSparse[h]*s->denseStride);

    K_ASSERT(pDense->pEnums[eComp].sparse != 0,
        "h: {i}, {i}, off: {sz}, sparse[h]: {i}",
        h, pDense->pEnums[eComp].sparse, ((uint8_t*)pDense - s->pDense)/s->denseStride, s->pSparse[h]
    );

    DenseEnum* pEnums = pDense->pEnums;

    const int enumDenseI = pEnums[eComp].sparse - 1;
    pEnums[enumDenseI].dense = pEnums[--pDense->enumsSize].dense; /* Swap with last. */
    pEnums[pEnums[enumDenseI].dense].sparse = enumDenseI + 1;
    pEnums[eComp].sparse = 0;

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
    --pComp->size;
}

void
ecs_MapRemoveEntity(ecs_Map* s, ECS_ENTITY h)
{
    K_ASSERT(h >= 0 && h < s->cap, "h: {i}, cap: {i}", h, s->cap);
    K_ASSERT(s->pSparse[h] != -1, "already deleted");
    DenseDesc* pDense = (DenseDesc*)(s->pDense + s->pSparse[h]*s->denseStride);

    while (pDense->enumsSize > 0)
        ecs_MapRemove(s, h, pDense->pEnums[0].dense);

    DenseDesc* pMoveDense = (DenseDesc*)(s->pDense + (s->size - 1)*s->denseStride);

    pDense->sparseI = pMoveDense->sparseI;
    pDense->enumsSize = pMoveDense->enumsSize;
    memcpy(pDense->pEnums, pMoveDense->pEnums, sizeof(*pMoveDense->pEnums)*pMoveDense->enumsSize);

    s->pSparse[pMoveDense->sparseI] = s->pSparse[h];
    s->pSparse[h] = -1;
    s->pFreeList[s->freeListSize++] = h;
    --s->size;
}

bool
ecs_MapAdd(ecs_Map* s, ECS_ENTITY h, int eComp, void* pVal)
{
    DenseDesc* pDense = (DenseDesc*)(s->pDense + s->pSparse[h]*s->denseStride);
    DenseEnum* pEnums = pDense->pEnums;

    K_ASSERT(pEnums[eComp].sparse == 0, "adding component({i}) twice, sparse: {i}, h: {i}, enumsSize: {i}, ({p})", eComp, pEnums[eComp].sparse, h, pDense->enumsSize, pEnums);
    pEnums[pDense->enumsSize].dense = eComp;
    pEnums[eComp].sparse = ++pDense->enumsSize; /* Sparse holds dense idx + 1. */

    /* Now add this entity to SOAComponents[eComp] map. */
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

    /* Push latest. */
    pComp->pSparse[h] = pComp->size;
    pComp->pDense[pComp->size] = h;

    uint8_t* pCompData = (uint8_t*)pComp->pData + (compSize*pComp->size);

    if (pVal) memcpy(pCompData, pVal, compSize);
    else memset(pCompData, 0, compSize);

    ++pComp->size;

    return true;
}

bool
ecs_MapHas(ecs_Map* s, ECS_ENTITY h, int eComp)
{
    DenseDesc* pDense = (DenseDesc*)(s->pDense + s->pSparse[h]*s->denseStride);
    return pDense->pEnums[eComp].sparse != 0;
}
