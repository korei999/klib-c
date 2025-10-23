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

typedef struct ComponentMap
{
    int cap;
    int size;
    int freeListSize;

    int* pDense;
    ENTITY_HANDLE* pSparse;
    int* pFreeList;
    ComponentList* pComponentLists;

    void* pSoaComponents[COMPONENT_ESIZE];
} ComponentMap;


static void
ComponentMapDestroy(ComponentMap* s)
{
    free(s->pDense);
}

static bool
ComponentMapGrow(ComponentMap* s, int newCap)
{
    ssize_t mainOff = sizeof(*s->pDense) + sizeof(*s->pSparse) + sizeof(*s->pFreeList) + sizeof(*s->pComponentLists);
    ssize_t totalCap = newCap * mainOff;
    for (ssize_t i = 0; i < K_ASIZE(COMPONENT_SIZE_MAP); ++i)
        totalCap += newCap*COMPONENT_SIZE_MAP[i];

    uint8_t* pNew = calloc(1, totalCap);
    if (!pNew) return false;
    void* pOld = s->pDense;

    {
        ssize_t off = 0;
        if (s->size > 0) memcpy(pNew, s->pDense, s->size*sizeof(*s->pDense));
        s->pDense = (int*)pNew;
        off += newCap * sizeof(*s->pDense);

        if (s->size > 0) memcpy(pNew + off, s->pSparse, s->size*sizeof(*s->pSparse));
        s->pSparse = (int*)(pNew + off);
        off += newCap * sizeof(*s->pSparse);

        if (s->size > 0) memcpy(pNew + off, s->pFreeList, s->size*sizeof(*s->pFreeList));
        s->pFreeList = (int*)(pNew + off);
        off += newCap * sizeof(*s->pFreeList);

        if (s->size > 0) memcpy(pNew + off, s->pComponentLists, s->size*sizeof(*s->pComponentLists));
        s->pComponentLists = (ComponentList*)(pNew + off);
    }

    for (ssize_t i = 0, off = mainOff * newCap; i < COMPONENT_ESIZE; ++i)
    {
        if (s->size > 0) memcpy(pNew + off, s->pSoaComponents[i], s->size*COMPONENT_SIZE_MAP[i]);
        s->pSoaComponents[i] = pNew + off;
        off += COMPONENT_SIZE_MAP[i]*newCap;
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
            return ENTITY_HANDLE_INVALID;
    }

    int i;

    if (s->freeListSize > 0) i = s->pFreeList[--s->freeListSize];
    else i = s->size;

    s->pDense[s->size] = i;
    s->pSparse[i] = s->size;
    s->pComponentLists[i].size = 0;

    ++s->size;
    return i;
}

static void
ComponentMapRemoveEntity(ComponentMap* s, ENTITY_HANDLE h)
{
    K_ASSERT(h >= 0 && h < s->cap && s->pSparse[h] != -1, "");
    s->pFreeList[s->freeListSize++] = h;

    const int thisSparseI = h;
    const int thisDenseI = s->pSparse[thisSparseI];
    const int newSparseI = s->pDense[thisDenseI] = s->pDense[s->size - 1];
    const int newDenseI = s->pSparse[newSparseI];

    ComponentList* pThisComp = &s->pComponentLists[thisDenseI];
    ComponentList* pNewComp = &s->pComponentLists[newDenseI];

    for (int i = 0; i < pNewComp->size; ++i)
    {
        COMPONENT thisCompI = pThisComp->aList[i] = pNewComp->aList[i];
        const ssize_t thisCompSize = COMPONENT_SIZE_MAP[thisCompI];

        memcpy(
            (uint8_t*)s->pSoaComponents[thisCompI] + thisDenseI*thisCompSize,
            (uint8_t*)s->pSoaComponents[thisCompI] + newDenseI*thisCompSize,
            thisCompSize
        );
    }
    pThisComp->size = pNewComp->size;
    pThisComp->mask = pNewComp->mask;
    pNewComp->size = 0;
    pNewComp->mask = 0;

    s->pSparse[newSparseI] = newDenseI;
    s->pSparse[thisSparseI] = -1;
    --s->size;
}

static void
ComponentMapAdd(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp, void* pVal)
{
    const int dI = s->pSparse[h];
    ComponentList* pCl = &s->pComponentLists[dI];

    K_ASSERT(!(pCl->mask & 1 << (eComp)), "Adding same component twice");
    pCl->mask |= 1 << (eComp);

    pCl->aList[pCl->size++] = eComp;
    uint8_t* pComp = s->pSoaComponents[eComp];
    memcpy(pComp + dI*COMPONENT_SIZE_MAP[eComp], pVal, COMPONENT_SIZE_MAP[eComp]);
}

static void*
ComponentMapGet(ComponentMap* s, ENTITY_HANDLE h, COMPONENT eComp)
{
    int denseI = s->pSparse[h];
    K_ASSERT(denseI >= 0 && denseI < s->size, "denseI: {i}, size: {i}", denseI, s->size);
    return (uint8_t*)s->pSoaComponents[eComp] + denseI*COMPONENT_SIZE_MAP[eComp];
}

static void*
ComponentMapAt(ComponentMap* s, int denseI, COMPONENT eComp)
{
    K_ASSERT(denseI >= 0 && denseI < s->size, "denseI: {i}, size: {i}", denseI, s->size);
    return (uint8_t*)s->pSoaComponents[eComp] + denseI*COMPONENT_SIZE_MAP[eComp];
}

static void
test(void)
{
    ComponentMap s = {0};

    ENTITY_HANDLE h0 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h1 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h2 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h3 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h4 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h5 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h6 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h7 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h8 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h9 = ComponentMapCreateEntity(&s);
    ENTITY_HANDLE h10 = ComponentMapCreateEntity(&s);

    {
        Health health0 = {77};
        Pos pos0 = {.x = 666, .y = 999};
        ComponentMapAdd(&s, h0, COMPONENT_POS, &pos0);
        ComponentMapAdd(&s, h0, COMPONENT_HEALTH, &health0);
    }

    {
        Health health1 = {12345};
        Pos pos1 = {.x = 111, .y = 222};
        ComponentMapAdd(&s, h1, COMPONENT_POS, &pos1);
        ComponentMapAdd(&s, h1, COMPONENT_HEALTH, &health1);
    }

    {
        Health health3 = {-99999};
        Pos pos2 = {.x = 3003, .y = 2002};
        ComponentMapAdd(&s, h2, COMPONENT_POS, &pos2);
        ComponentMapAdd(&s, h2, COMPONENT_HEALTH, &health3);
    }

    {
        Health health10 = {10};
        Pos pos10 = {.x = 10, .y = 10};
        ComponentMapAdd(&s, h10, COMPONENT_POS, &pos10);
        ComponentMapAdd(&s, h10, COMPONENT_HEALTH, &health10);
    }

    {
        Pos* pPos0 = ComponentMapGet(&s, h0, COMPONENT_POS);
        Health* pHealth0 = ComponentMapGet(&s, h0, COMPONENT_HEALTH);
        Pos* pPos1 = ComponentMapGet(&s, h1, COMPONENT_POS);
        Pos* pPos2 = ComponentMapGet(&s, h2, COMPONENT_POS);

        K_CTX_LOG_DEBUG("pPos0: {:.3:f}, {:.3:f}", pPos0->x, pPos0->y);
        K_CTX_LOG_DEBUG("pHealth0: {i}", pHealth0->val);
        K_CTX_LOG_DEBUG("pPos1: {:.3:f}, {:.3:f}", pPos1->x, pPos1->y);
        K_CTX_LOG_DEBUG("pPos2: {:.3:f}, {:.3:f}", pPos2->x, pPos2->y);
    }

    // K_CTX_LOG_DEBUG("size: {i}", s.size);

    ComponentMapRemoveEntity(&s, h1);
    ComponentMapRemoveEntity(&s, h10);
    ComponentMapRemoveEntity(&s, h9);
    ComponentMapRemoveEntity(&s, h8);
    ComponentMapRemoveEntity(&s, h7);
    ComponentMapRemoveEntity(&s, h6);
    ComponentMapRemoveEntity(&s, h5);
    ComponentMapRemoveEntity(&s, h4);

    ENTITY_HANDLE h11 = ComponentMapCreateEntity(&s);
    {
        Health health11 = {-1111};
        Pos pos11 = {.x = -1111, .y = -1111};
        ComponentMapAdd(&s, h11, COMPONENT_POS, &pos11);
        // ComponentMapAdd(&s, h11, COMPONENT_HEALTH, &health11);
    }
    ComponentMapRemoveEntity(&s, h0);

    {
        Pos* pPos = ComponentMapAt(&s, 0, COMPONENT_POS);
        Health* pHealth = ComponentMapAt(&s, 0, COMPONENT_HEALTH);
        for (int i = 0; i < s.size; ++i)
        {
            K_CTX_LOG_INFO("{i} pos: ({:.3:f}, {:.3:f}), health: {i}", i, pPos[i].x, pPos[i].y, pHealth[i].val);
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
