#include "ecs.h"

#include "klib/Ctx.h"

/* Entity stores info about what components it has in array of DenseEnums.
 * Using uint8_t we have up to 254 (0 for invalid sparse index) components per entity.
 * ComponentDescSparseIndices + ComponentDescDenseIndices are stored continuously in one allocation with ComponentDesc. */
typedef struct
{
    int sparseI; /* Back to sparse mapping. */
    int nComponents;
    /* ComponentDescSparseIndices of sizeMapSize capacity */
    /* ComponentDescDenseIndices of sizeMapSize capacity */
} ComponentDesc;

static inline uint8_t*
ComponentDescSparseIndices(ComponentDesc* s)
{
    return (uint8_t*)s + sizeof(ComponentDesc);
}

/* Back to ComponentDescSparseIndices */
static inline uint8_t*
ComponentDescDenseIndices(ComponentDesc* s, const ecs_Map* pMap)
{
    return ComponentDescSparseIndices(s) + sizeof(uint8_t)*pMap->sizeMapSize;
}

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

    s->denseStride = sizeof(ComponentDesc) + K_ALIGN_UP(sizeof(uint8_t)*2*sizeMapSize, _Alignof(ComponentDesc));

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
    k_IAllocatorFree(s->pAlloc, s->pDenseDesc);
}

static bool
MapGrow(ecs_Map* s, int newCap)
{
    const ssize_t totalCap = newCap * (
        s->denseStride +
        sizeof(*s->pSparse) +
        sizeof(*s->pGenerations) +
        sizeof(*s->pFreeList) +
        sizeof(*s->pSOAComponents[0].pSparse)*s->sizeMapSize
    );
    uint8_t* pNew = k_IAllocatorZalloc(s->pAlloc, totalCap);
    if (!pNew) return false;
    void* pOld = s->pDenseDesc;

    ssize_t off = 0;

    {
        if (s->pDenseDesc) memcpy(pNew, s->pDenseDesc, s->denseStride*s->size);
        s->pDenseDesc = pNew;
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
        if (s->pGenerations) memcpy(pNew + off, s->pGenerations, sizeof(*s->pGenerations)*s->cap);
        s->pGenerations = (void*)(pNew + off);
        off += sizeof(*s->pGenerations)*newCap;
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

ecs_Entity
ecs_MapAddEntity(ecs_Map* s)
{
    if (s->size >= s->cap)
    {
        if (!MapGrow(s, K_MAX(8, s->cap * 2)))
            return (ecs_Entity){.id = -1, .gen = 0};
    }

    int sparseI = s->freeListSize > 0 ? s->pFreeList[--s->freeListSize] : s->sparseSize++;
    s->pSparse[sparseI] = s->size;
    ComponentDesc* pDense = (ComponentDesc*)(s->pDenseDesc + s->denseStride*s->size);
    memset(pDense, 0, s->denseStride);
    pDense->sparseI = sparseI;
    ++s->size;

    return (ecs_Entity){.id = sparseI, ++s->pGenerations[sparseI]};
}

void
ecs_MapRemove(ecs_Map* s, ecs_Entity h, int eComp)
{
    ComponentDesc* pDesc = (ComponentDesc*)(s->pDenseDesc + s->pSparse[h.id]*s->denseStride);

    uint8_t* pDescSparse = ComponentDescSparseIndices(pDesc);
    uint8_t* pDescDense = ComponentDescDenseIndices(pDesc, s);
    K_ASSERT(pDescSparse[eComp] != 0, "already removed");

    /* SparseI is eComp. */
    const uint8_t thisDenseI = pDescSparse[eComp] - 1;

    /* Swap this dense with last dense, and update last dense sparse index (confusing af). */
    pDescDense[thisDenseI] = pDescDense[pDesc->nComponents - 1];
    pDescSparse[pDescDense[thisDenseI]] = thisDenseI + 1;

    pDescSparse[eComp] = 0;

    --pDesc->nComponents;

    ecs_Component* pComp = &s->pSOAComponents[eComp];
    const ssize_t compSize = s->pSizeMap[eComp];

    const int denseI = pComp->pSparse[h.id];
    const int moveSparseI = pComp->pDense[pComp->size - 1];
    const int moveDenseI = pComp->pSparse[moveSparseI];

    K_ASSERT(pComp->pDense[denseI] == h.id, "sparseI: {i}, h.id: {i}", pComp->pDense[denseI], h.id);

    if (moveDenseI != denseI)
    {
        memcpy((uint8_t*)pComp->pData + denseI*compSize, (uint8_t*)pComp->pData + moveDenseI*compSize, compSize);
        pComp->pSparse[moveSparseI] = denseI;
    }

    pComp->pDense[denseI] = moveSparseI;

    pComp->pSparse[h.id] = -1;
    --pComp->size;
}

void
ecs_MapRemoveEntity(ecs_Map* s, ecs_Entity h)
{
    K_ASSERT(h.gen == s->pGenerations[h.id], "generational mismatch: {id: {i}. gen: {i}}", h.id, h.gen);
    K_ASSERT(h.id >= 0 && h.id < s->cap, "h.id: {i}, cap: {i}", h.id, s->cap);
    K_ASSERT(s->pSparse[h.id] != -1, "already deleted");
    ComponentDesc* pDesc = (ComponentDesc*)(s->pDenseDesc + s->pSparse[h.id]*s->denseStride);

    uint8_t* pDescDense = ComponentDescDenseIndices(pDesc, s);
    while (pDesc->nComponents > 0)
        ecs_MapRemove(s, h, pDescDense[0]);

    ComponentDesc* pMoveDense = (ComponentDesc*)(s->pDenseDesc + (s->size - 1)*s->denseStride);

    if (pDesc != pMoveDense)
    {
        memcpy(pDesc, pMoveDense, sizeof(ComponentDesc) + s->sizeMapSize + pMoveDense->nComponents);
        s->pSparse[pMoveDense->sparseI] = s->pSparse[h.id];
    }

    s->pSparse[h.id] = -1;
    s->pFreeList[s->freeListSize++] = h.id;
    --s->size;
}

bool
ecs_MapAdd(ecs_Map* s, ecs_Entity h, int eComp, void* pVal)
{
    K_ASSERT(h.gen == s->pGenerations[h.id], "generational mismatch: {id: {i}. gen: {i}}", h.id, h.gen);

    ComponentDesc* pDesc = (ComponentDesc*)(s->pDenseDesc + s->pSparse[h.id]*s->denseStride);
    uint8_t* pCompSparse = ComponentDescSparseIndices(pDesc);
    uint8_t* pCompDense = ComponentDescDenseIndices(pDesc, s);

    K_ASSERT(pDesc->sparseI == h.id, "dense isn't mapped back to sparse correctly, {i} -> {i}", h.id, pDesc->sparseI);

    pCompSparse[eComp] = pDesc->nComponents + 1;
    pCompDense[pDesc->nComponents] = eComp;
    ++pDesc->nComponents;

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

    /* Append. */
    pComp->pSparse[h.id] = pComp->size;
    pComp->pDense[pComp->size] = h.id;

    uint8_t* pCompData = (uint8_t*)pComp->pData + (compSize*pComp->size);

    if (pVal) memcpy(pCompData, pVal, compSize);
    else memset(pCompData, 0, compSize);

    ++pComp->size;

    return true;
}

bool
ecs_MapHas(ecs_Map* s, ecs_Entity h, int eComp)
{
    ComponentDesc* pDense = (ComponentDesc*)(s->pDenseDesc + s->pSparse[h.id]*s->denseStride);
    return ComponentDescSparseIndices(pDense)[eComp] != 0;
}

ssize_t
ecs_EntityFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* p)
{
    ecs_Entity en;
    memcpy(&en, &p, sizeof(p));
    return k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
        "(id: {i}, gen: {u32})", en.id, en.gen
    ).size;
}

void
ecs_DBG_PrintDenseComponents(ecs_Map* s, ecs_Entity h)
{
    ComponentDesc* pDense = (ComponentDesc*)(s->pDenseDesc + s->pSparse[h.id]*s->denseStride);
    uint8_t* pCompDense = ComponentDescDenseIndices(pDense, s);

    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder pb = {0};
        k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base});
        if (pDense->nComponents > 0)
            k_print_BuilderPrint(&pb, "{u8}", pCompDense[0]);
        for (int i = 1; i < pDense->nComponents; ++i)
            k_print_BuilderPrint(&pb, ", {u8}", pCompDense[i]);
        k_StringView sv = k_print_BuilderToSv(&pb);
        K_CTX_LOG_DEBUG("entity(id: {i}, gen: {i}), has these components: {PSv}", h.id, h.gen, &sv);
    }
}

void*
ecs_MapAt(ecs_Map* s, int denseI, int eComp)
{
    K_ASSERT(eComp >= 0 && eComp < s->sizeMapSize, "");
    K_ASSERT(denseI >= 0 && denseI < s->cap, "denseI: {i}, cap: {i}", denseI, s->cap);
    ComponentDesc* pDense = (ComponentDesc*)(s->pDenseDesc + denseI*s->denseStride);
    const int sparseI = pDense->sparseI;
    K_ASSERT(sparseI != -1, "sparse index {i} (dense: {i}) is deleted", sparseI, denseI);

    ecs_Component* pComp = &s->pSOAComponents[eComp];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}
