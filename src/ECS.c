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
    COMPONENT_MASK mask;
    uint8_t compListSize;
    uint8_t aCompEnumIdxs[COMPONENT_ESIZE];
    int aCompSparseIdxs[COMPONENT_ESIZE];
} DenseMap;

typedef struct SOAComponent
{
    void* pData; /* pData and pDense are both allocated with one calloc(). */
    int* pDense;
    int size;
    int cap;
    int* pSparse; /* Has ComponentMap::cap capacity and allocated alongside with the ComponentMap. */
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
}

static bool
ComponentMapGrow(ComponentMap* s, int newCap)
{
    const ssize_t totalCap = newCap * (sizeof(*s->pDense) + sizeof(*s->pSparse) + sizeof(*s->pFreeList) + sizeof(*s->aSOAComponents[0].pSparse)*COMPONENT_ESIZE);
    uint8_t* pNew = calloc(1, totalCap);
    if (!pNew) return false;
    void* pOld = s->pDense;

    ssize_t off = 0;

    if (s->pDense) memcpy(pNew, s->pDense, sizeof(*s->pDense)*s->size);
    s->pDense = (void*)(pNew);
    off += sizeof(*s->pDense)*newCap;

    if (s->pSparse) memcpy(pNew + off, s->pSparse, sizeof(*s->pSparse)*s->cap);
    s->pSparse = (void*)(pNew + off);
    for (int i = s->cap; i < newCap; ++i) s->pSparse[i] = -1;
    off += sizeof(*s->pSparse)*newCap;

    if (s->pFreeList) memcpy(pNew + off, s->pFreeList, sizeof(*s->pFreeList)*s->cap);
    s->pSparse = (void*)(pNew + off);
    off += sizeof(*s->pFreeList)*newCap;

    for (ssize_t i = 0; i < COMPONENT_ESIZE; ++i)
    {
        memcpy(pNew + off, s->aSOAComponents[i].pSparse, sizeof(*s->aSOAComponents[0].pSparse)*s->cap);
        s->aSOAComponents[i].pSparse = (void*)(pNew + off);
        off += sizeof(*s->aSOAComponents[0].pSparse)*newCap;
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

    int i;
    if (s->freeListSize > 0) i = s->pFreeList[--s->freeListSize];
    else i = s->size++;

    return i;
}

static void
ComponentMapRemoveEntity(ComponentMap* s, ENTITY_HANDLE h)
{
}

static bool
ComponentMapAdd(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp, void* pVal)
{
    DenseMap* pDense = &s->pDense[s->pSparse[h]];

    assert(!(pDense->mask & 1 << eComp) && "trying to add same component twice");
    pDense->mask |= 1 << eComp;
    pDense->aCompEnumIdxs[pDense->compListSize++] = eComp;

    /* Now we need to add this entity to the SOAComponent[eComp] map. */
    SOAComponent* pComp = &s->aSOAComponents[eComp];

    if (pComp->size >= pComp->cap)
    {
        const int newCap = K_MAX(8, pComp->cap * 2);
        uint8_t* pNew = calloc(1, newCap * sizeof(*pComp->pDense) + COMPONENT_SIZE_MAP[eComp]);
        if (!pNew) return false;
        if (pComp->pData)
        {
            memcpy(pNew, pComp->pData, COMPONENT_SIZE_MAP[eComp]*pComp->size);
            memcpy(pNew + COMPONENT_SIZE_MAP[eComp]*newCap, pComp->pDense, sizeof(*pComp->pDense)*pComp->size);
        }
        free(pComp->pData);
        pComp->pData = pNew;
        pComp->pDense = (void*)(pNew + COMPONENT_SIZE_MAP[eComp]*newCap);
        pComp->cap = newCap;
    }

    const int compDenseI = pComp->pSparse[h];
    pComp->pDense[compDenseI] = h;

    return true;
}

static void*
ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp)
{
    return NULL;
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

    ENTITY_HANDLE h0 = ComponentMapCreateEntity(&s);

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
