#include "klib/Ctx.h"
#include "klib/assert.h"

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
    COMPONENT_ESIZE
} COMPONENT;

static const ssize_t COMPONENT_SIZE_MAP[COMPONENT_ESIZE] = {
    sizeof(Pos),
    sizeof(Health),
};

typedef uint8_t COMPONENT_MASK;
_Static_assert(sizeof(COMPONENT_MASK)*8 > COMPONENT_ESIZE, "");

typedef struct ComponentList
{
    COMPONENT_MASK mask;
    uint8_t size;
    uint8_t aList[COMPONENT_ESIZE];
} ComponentList;

typedef int ENTITY_HANDLE;
static const ENTITY_HANDLE ENTITY_HANDLE_INVALID = -1;

typedef struct DenseMap
{
    int sparseI;
    uint8_t enumsSize;
    int aEnumDense[COMPONENT_ESIZE];
    int16_t aEnumSparse[COMPONENT_ESIZE]; /* Holds dense index + 1, so that invalid index is 0. */
} DenseMap;

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
    DenseMap* pDense;
    ENTITY_HANDLE* pSparse;
    int* pFreeList;
    SOAComponent aSOAComponents[COMPONENT_ESIZE];

    int size;
    int cap;
    int freeListSize;
} ComponentMap;

static void
ComponentMapDestroy(ComponentMap* s)
{
    for (ssize_t i = 0; i < K_ASIZE(s->aSOAComponents); ++i)
        if (s->aSOAComponents[i].cap > 0) free(s->aSOAComponents[i].pData);

    free(s->pDense);
}

static bool
ComponentMapGrow(ComponentMap* s, int newCap)
{
    const ssize_t totalCap = newCap * (
        sizeof(*s->pDense) +
        sizeof(*s->pSparse) +
        sizeof(*s->pFreeList) +
        sizeof(*s->aSOAComponents[0].pSparse)*COMPONENT_ESIZE +
        sizeof(*s->aSOAComponents[0].pFreeList)*COMPONENT_ESIZE
    );
    uint8_t* pNew = calloc(1, totalCap);
    if (!pNew) return false;
    void* pOld = s->pDense;

    ssize_t off = 0;

    {
        if (s->pDense) memcpy(pNew, s->pDense, sizeof(*s->pDense)*s->size);
        s->pDense = (void*)(pNew);
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

    for (ssize_t compI = 0; compI < COMPONENT_ESIZE; ++compI)
    {
        if (s->aSOAComponents[compI].pSparse)
            memcpy(pNew + off, s->aSOAComponents[compI].pSparse, sizeof(*s->aSOAComponents[0].pSparse)*s->cap);

        s->aSOAComponents[compI].pSparse = (void*)(pNew + off);
        off += sizeof(*s->aSOAComponents[0].pSparse)*newCap;
        for (int i = s->cap; i < newCap; ++i)
            s->aSOAComponents[compI].pSparse[i] = -1;

        if (s->aSOAComponents[compI].pFreeList)
            memcpy(pNew + off, s->aSOAComponents[compI].pFreeList, sizeof(*s->aSOAComponents[0].pFreeList)*s->cap);

        s->aSOAComponents[compI].pFreeList = (void*)(pNew + off);
        off += sizeof(*s->aSOAComponents[0].pFreeList)*newCap;
    }

    free(pOld);
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
ComponentMapRemove(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp)
{
    DenseMap* pDense = &s->pDense[s->pSparse[h]];

    K_ASSERT(pDense->aEnumSparse[eComp] != 0, "");

    const int enumDenseI = pDense->aEnumSparse[eComp] - 1;
    pDense->aEnumDense[enumDenseI] = pDense->aEnumDense[--pDense->enumsSize]; /* Swap with last. */
    pDense->aEnumSparse[pDense->aEnumDense[enumDenseI]] = enumDenseI + 1;
    pDense->aEnumSparse[eComp] = 0;

    SOAComponent* pSOA = &s->aSOAComponents[eComp];
    const ssize_t compSize = COMPONENT_SIZE_MAP[eComp];

    const int denseI = pSOA->pSparse[h];
    const int sparseI = pSOA->pDense[denseI];
    const int moveSparseI = pSOA->pDense[pSOA->size - 1];
    const int moveDenseI = pSOA->pSparse[moveSparseI];

    memcpy((uint8_t*)pSOA->pData + denseI*compSize, (uint8_t*)pSOA->pData + moveDenseI*compSize, compSize);
    pSOA->pDense[moveDenseI] = moveSparseI;
    pSOA->pSparse[h] = moveDenseI;
    pSOA->pFreeList[pSOA->freeListSize++] = denseI;
    --pSOA->size;
}

static void
ComponentMapRemoveEntity(ComponentMap* s, ENTITY_HANDLE h)
{
    DenseMap* pDense = &s->pDense[s->pSparse[h]];

    for (int compI = 0; compI < pDense->enumsSize; ++compI)
    {
        const COMPONENT eComp = pDense->aEnumDense[compI];
        SOAComponent* pSOA = &s->aSOAComponents[eComp];
        const ssize_t compSize = COMPONENT_SIZE_MAP[eComp];

        const int denseI = pSOA->pSparse[h];
        const int sparseI = pSOA->pDense[denseI];
        const int moveSparseI = pSOA->pDense[pSOA->size - 1];
        const int moveDenseI = pSOA->pSparse[moveSparseI];

        memcpy((uint8_t*)pSOA->pData + denseI*compSize, (uint8_t*)pSOA->pData + moveDenseI*compSize, compSize);
        pSOA->pDense[moveDenseI] = moveSparseI;
        pSOA->pSparse[h] = moveDenseI;
        pSOA->pFreeList[pSOA->freeListSize++] = denseI;
        --pSOA->size;
    }

    DenseMap* pMoveDense = &s->pDense[s->size - 1];
    const int moveSparseI = pMoveDense->sparseI;
    memcpy(pDense, pMoveDense, sizeof(*pMoveDense));
    s->pSparse[h] = pMoveDense->sparseI;
    s->pFreeList[s->freeListSize++] = s->pSparse[h];
    --s->size;
}

static bool
ComponentMapAdd(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp, void* pVal)
{
    DenseMap* pDense = &s->pDense[s->pSparse[h]];

    K_ASSERT(pDense->aEnumSparse[eComp] == 0, "");
    pDense->aEnumDense[pDense->enumsSize] = eComp;
    pDense->aEnumSparse[eComp] = ++pDense->enumsSize; /* Sparse holds dense idx + 1. */

    /* Now we need to add this entity to the SOAComponents[eComp] map. */
    SOAComponent* pComp = &s->aSOAComponents[eComp];
    const ssize_t compSize = COMPONENT_SIZE_MAP[eComp];

    if (pComp->size >= pComp->cap)
    {
        const int newCap = K_MAX(8, pComp->cap * 2);
        uint8_t* pNew = calloc(1, newCap*(sizeof(*pComp->pDense) + compSize));
        if (!pNew) return false;
        if (pComp->pData)
        {
            memcpy(pNew, pComp->pData, compSize*pComp->size);
            memcpy(pNew + compSize*newCap, pComp->pDense, sizeof(*pComp->pDense)*pComp->size);
        }
        free(pComp->pData);
        pComp->pData = pNew;
        pComp->pDense = (void*)(pNew + compSize*newCap);
        pComp->cap = newCap;
    }

    const int compDenseI = pComp->freeListSize > 0 ? pComp->pFreeList[--pComp->freeListSize] : pComp->size++;
    pComp->pSparse[h] = compDenseI;
    pComp->pDense[compDenseI] = h;
    if (pVal) memcpy((uint8_t*)pComp->pData + compDenseI*compSize, pVal, compSize);
    else memset((uint8_t*)pComp->pData + compDenseI*compSize, 0, compSize);

    return true;
}

static void*
ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp)
{
    SOAComponent* pSOA = &s->aSOAComponents[eComp];
    const int denseI = pSOA->pSparse[h];
    return (uint8_t*)pSOA->pData + denseI*COMPONENT_SIZE_MAP[eComp];
}

static void*
ComponentMapAt(ComponentMap* s, int denseI, COMPONENT eComp)
{
    return NULL;
}

static void
test(void)
{
    ComponentMap s = {0};
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

    // ComponentMapRemoveEntity(&s, aH[3]);
    ComponentMapRemove(&s, aH[4], COMPONENT_HEALTH);

    {
        Pos* pPos = s.aSOAComponents[COMPONENT_POS].pData;
        for (int posI = 0; posI < s.aSOAComponents[COMPONENT_POS].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) pos: ({:.3:f}, {:.3:f})",
                s.aSOAComponents[COMPONENT_POS].pDense[posI], pPos[posI].x, pPos[posI].y
            );
        }
        K_CTX_LOG_DEBUG("");
        Health* pHealth = s.aSOAComponents[COMPONENT_HEALTH].pData;
        for (int posI = 0; posI < s.aSOAComponents[COMPONENT_HEALTH].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) Health: {i}",
                s.aSOAComponents[COMPONENT_HEALTH].pDense[posI], pHealth[posI].val
            );
        }

        {
            Pos* p3 = ComponentMapGet(&s, aH[3], COMPONENT_POS);
            Pos* p4 = ComponentMapGet(&s, aH[4], COMPONENT_POS);
            K_CTX_LOG_DEBUG("h3.pos: ({f}, {f})", p3->x, p3->y);
            K_CTX_LOG_DEBUG("h4.pos: ({f}, {f})", p4->x, p4->y);
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
