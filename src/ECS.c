#include "klib/Ctx.h"
#include "klib/assert.h"
#include "klib/Gpa.h"

typedef int ENTITY_HANDLE;
static const ENTITY_HANDLE ENTITY_HANDLE_INVALID = -1;

typedef struct DenseEnum
{
    uint8_t dense;
    uint8_t sparse; /* Holds dense index + 1, such that invalid index is 0. */
} DenseEnum;

typedef struct DenseDesc
{
    int sparseI;
    uint8_t enumsSize;
    DenseEnum* pEnums;
} DenseDesc;

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

    DenseDesc* pDense;
    ENTITY_HANDLE* pSparse;
    int* pFreeList;
    SOAComponent* pSOAComponents;

    int size;
    int cap;
    int freeListSize;

    const int* pSizeMap;
    int sizeMapSize;
} ComponentMap;

static bool ComponentMapGrow(ComponentMap* s, int newCap);

static bool
ComponentMapInit(ComponentMap* s, k_IAllocator* pAlloc, int cap, const int* pSizeMap, int sizeMapSize)
{
    s->pAlloc = pAlloc;
    s->pSizeMap = pSizeMap;
    s->sizeMapSize = sizeMapSize;

    void* pSOA = k_IAllocatorZalloc(pAlloc, sizeMapSize * sizeof(*s->pSOAComponents));
    if (!pSOA) return false;
    s->pSOAComponents = pSOA;

    if (!ComponentMapGrow(s, cap))
    {
        k_IAllocatorFree(pAlloc, pSOA);
        s->pSOAComponents = NULL;
        return false;
    }

    return true;
}

static void
ComponentMapDestroy(ComponentMap* s)
{
    for (int i = 0; i < s->cap; ++i)
        k_IAllocatorFree(s->pAlloc, s->pDense[i].pEnums);
    for (ssize_t i = 0; i < s->sizeMapSize; ++i)
        k_IAllocatorFree(s->pAlloc, s->pSOAComponents[i].pData);
    k_IAllocatorFree(s->pAlloc, s->pSOAComponents);
    k_IAllocatorFree(s->pAlloc, s->pDense);
}

static bool
ComponentMapGrow(ComponentMap* s, int newCap)
{
    const ssize_t totalCap = newCap * (
        sizeof(*s->pDense) +
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
        if (s->pDense) memcpy(pNew, s->pDense, sizeof(*s->pDense)*s->size);
        s->pDense = (void*)(pNew);

        for (int i = s->cap; i < newCap; ++i)
            s->pDense[i].pEnums = k_IAllocatorZalloc(s->pAlloc, sizeof(*s->pDense[i].pEnums) * s->sizeMapSize);

        off += sizeof(*s->pDense)*newCap;
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

static ENTITY_HANDLE
ComponentMapCreateEntity(ComponentMap* s)
{
    if (s->size >= s->cap)
    {
        if (!ComponentMapGrow(s, K_MAX(8, s->cap * 2)))
            return -1;
    }

    int i = s->freeListSize > 0 ? s->pFreeList[--s->freeListSize] : s->size;

    s->pSparse[s->size] = i;
    s->pDense[i].sparseI = s->size;
    s->pDense[i].enumsSize = 0;

    ++s->size;

    return i;
}

static void
ComponentMapRemove(ComponentMap* s, ENTITY_HANDLE h, int eComp)
{
    DenseDesc* pDense = &s->pDense[s->pSparse[h]];

    K_ASSERT(pDense->pEnums[eComp].sparse != 0, "h: {i}, {i}, off: {sz}, sparse[h]: {i}", h, pDense->pEnums[eComp].sparse, pDense - s->pDense, s->pSparse[h]);

    const int enumDenseI = pDense->pEnums[eComp].sparse - 1;
    pDense->pEnums[enumDenseI].dense = pDense->pEnums[--pDense->enumsSize].dense; /* Swap with last. */
    pDense->pEnums[pDense->pEnums[enumDenseI].dense].sparse = enumDenseI + 1;
    pDense->pEnums[eComp].sparse = 0;

    SOAComponent* pComp = &s->pSOAComponents[eComp];
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

static void
ComponentMapRemoveEntity(ComponentMap* s, ENTITY_HANDLE h)
{
    DenseDesc* pDense = &s->pDense[s->pSparse[h]];

    while (pDense->enumsSize > 0)
        ComponentMapRemove(s, h, pDense->pEnums[0].dense);

    DenseDesc* pMoveDense = &s->pDense[s->size - 1];

    pDense->sparseI = pMoveDense->sparseI;
    pDense->enumsSize = pMoveDense->enumsSize;
    memcpy(pDense->pEnums, pMoveDense->pEnums, sizeof(*pMoveDense->pEnums)*pMoveDense->enumsSize);

    s->pSparse[pMoveDense->sparseI] = s->pSparse[h];
    s->pSparse[h] = -1;
    s->pFreeList[s->freeListSize++] = s->pSparse[h];
    --s->size;
}

static bool
ComponentMapAdd(ComponentMap* s, ENTITY_HANDLE h, int eComp, void* pVal)
{
    DenseDesc* pDense = &s->pDense[s->pSparse[h]];

    K_ASSERT(pDense->pEnums[eComp].sparse == 0, "");
    pDense->pEnums[pDense->enumsSize].dense = eComp;
    pDense->pEnums[eComp].sparse = ++pDense->enumsSize; /* Sparse holds dense idx + 1. */

    /* Now we need to add this entity to the SOAComponents[eComp] map. */
    SOAComponent* pComp = &s->pSOAComponents[eComp];
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

static void*
ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, int eComp)
{
    SOAComponent* pComp = &s->pSOAComponents[eComp];
    const int denseI = pComp->pSparse[h];
    return (uint8_t*)pComp->pData + denseI*s->pSizeMap[eComp];
}

static void*
ComponentMapAt(ComponentMap* s, int denseI, int eComp)
{
    return NULL;
}

typedef struct Pos
{
    float x;
    float y;
} Pos;

typedef struct Health
{
    int val;
} Health;

typedef enum COMPONENT
{
    COMPONENT_POS,
    COMPONENT_HEALTH,
} COMPONENT;

static const int COMPONENT_SIZE_MAP[] = {
    [COMPONENT_POS] = sizeof(Pos),
    [COMPONENT_HEALTH] = sizeof(Health),
};

static void
test(void)
{
    ComponentMap s = {0};
    ComponentMapInit(&s, &k_GpaInst()->base, 8, COMPONENT_SIZE_MAP, K_ASIZE(COMPONENT_SIZE_MAP));
    ENTITY_HANDLE aH[17] = {0};

    for (ssize_t i = 0; i < K_ASIZE(aH) - 1; ++i)
        aH[i] = ComponentMapCreateEntity(&s);

    {
        Pos p3 = {.x = 3, .y = -3};
        ComponentMapAdd(&s, aH[3], COMPONENT_POS, &p3);
    }
    {
        Pos p4 = {.x = 4, .y = -4};
        Health hl4 = {4};
        ComponentMapAdd(&s, aH[4], COMPONENT_POS, &p4);
        ComponentMapAdd(&s, aH[4], COMPONENT_HEALTH, &hl4);
    }

    {
        aH[16] = ComponentMapCreateEntity(&s);
        ComponentMapAdd(&s, aH[16], COMPONENT_POS, &(Pos){.x = 16, .y = -16});
        ComponentMapAdd(&s, aH[16], COMPONENT_HEALTH, &(Health){16});
    }

    {
        ComponentMapAdd(&s, aH[11], COMPONENT_POS, &(Pos){11, -11});
        ComponentMapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});
        ComponentMapAdd(&s, aH[13], COMPONENT_POS, &(Pos){13, -13});
        ComponentMapAdd(&s, aH[13], COMPONENT_HEALTH, &(Health){13});
    }

    ComponentMapRemoveEntity(&s, aH[4]);
    ComponentMapRemoveEntity(&s, aH[16]);

    ComponentMapRemove(&s, aH[11], COMPONENT_HEALTH);
    ComponentMapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});

    {
        Pos* pPos = s.pSOAComponents[COMPONENT_POS].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_POS].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) pos: ({:.3:f}, {:.3:f})",
                s.pSOAComponents[COMPONENT_POS].pDense[posI], pPos[posI].x, pPos[posI].y
            );
        }
        K_CTX_LOG_DEBUG("");
        Health* pHealth = s.pSOAComponents[COMPONENT_HEALTH].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_HEALTH].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) Health: {i}",
                s.pSOAComponents[COMPONENT_HEALTH].pDense[posI], pHealth[posI].val
            );
        }

        {
            Health* pH13 = ComponentMapGet(&s, aH[13], COMPONENT_HEALTH);
            Pos* p13 = ComponentMapGet(&s, aH[13], COMPONENT_POS);
            K_CTX_LOG_DEBUG("h13 Health: {i}, p13 pos: ({:.3:f}, {:.3:f})", pH13->val, p13->x, p13->y);

            Health* pH11 = ComponentMapGet(&s, aH[11], COMPONENT_HEALTH);
            K_CTX_LOG_DEBUG("h11 Health: {i}", pH11->val);
        }
    }

    ComponentMapDestroy(&s);
}

int
main(void)
{
    k_CtxInitGlobal(
        (k_LoggerInitOpts){
            .bPrintSource = true,
            .bPrintTime = false,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = 2,
            .eLogLevel = K_LOG_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    test();

    k_CtxDestroyGlobal();
}
