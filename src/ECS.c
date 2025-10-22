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

static const ssize_t COMPONENT_SIZES[COMPONENT_ESIZE] = {
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
static ENTITY_HANDLE ENTITY_HANDLE_INVALID = -1;

typedef struct Components
{
    int cap;
    int size;
    int freeListSize;

    int* pDense;
    int* pSparse;
    int* pFreeList;
    ComponentList* pComponentLists;

    void* pComponents[COMPONENT_ESIZE];
} Components;


static void
ComponentsMapDestroy(Components* s)
{
    free(s->pDense);
}

static bool
ComponentsMapGrow(Components* s, int newCap)
{
    ssize_t mainOff = sizeof(*s->pDense) + sizeof(*s->pSparse) + sizeof(*s->pFreeList) + sizeof(*s->pComponentLists);
    ssize_t totalCap = newCap * mainOff;
    for (ssize_t i = 0; i < K_ASIZE(COMPONENT_SIZES); ++i)
        totalCap += newCap*COMPONENT_SIZES[i];

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
        if (s->pComponents[i]) memcpy(pNew + off, s->pComponents[i], s->size*COMPONENT_SIZES[i]);
        s->pComponents[i] = pNew + off;
        off += COMPONENT_SIZES[i]*newCap;
    }

    free(pOld);
    s->cap = newCap;

    return true;
}

static ENTITY_HANDLE
EntityCreate(Components* s)
{
    if (s->size >= s->cap)
    {
        if (!ComponentsMapGrow(s, K_MAX(8, s->cap * 2)))
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
EntityRemove(ENTITY_HANDLE h, Components* s)
{
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
        const ssize_t thisCompSize = COMPONENT_SIZES[thisCompI];

        memcpy(
            (uint8_t*)s->pComponents[thisCompI] + thisDenseI*thisCompSize,
            (uint8_t*)s->pComponents[thisCompI] + newDenseI*thisCompSize,
            thisCompSize
        );
    }
    pThisComp->size = pNewComp->size;
    pNewComp->size = 0;
    s->pSparse[thisSparseI] = newSparseI;
    --s->size;
}

static void
EntityAddComponent(ENTITY_HANDLE h, Components* s, COMPONENT eComp, void* pVal)
{
    const int dI = s->pSparse[h];
    ComponentList* pCl = &s->pComponentLists[dI];

    K_ASSERT(!(pCl->mask & 1 << (eComp + 1)), "");
    pCl->mask |= 1 << (eComp + 1);

    pCl->aList[pCl->size++] = eComp;
    uint8_t* pComp = s->pComponents[eComp];
    memcpy(pComp + dI*COMPONENT_SIZES[eComp], pVal, COMPONENT_SIZES[eComp]);
}

static void*
EntityGetComponent(ENTITY_HANDLE h, Components* s, COMPONENT eComp)
{
    return (uint8_t*)s->pComponents[eComp] + s->pSparse[h]*COMPONENT_SIZES[eComp];
}

static void*
ComponentAt(int denseI, Components* s, COMPONENT eComp)
{
    return (uint8_t*)s->pComponents[eComp] + denseI*COMPONENT_SIZES[eComp];
}

static void
test(void)
{
    Components s = {0};

    ENTITY_HANDLE h0 = EntityCreate(&s);
    ENTITY_HANDLE h1 = EntityCreate(&s);
    ENTITY_HANDLE h2 = EntityCreate(&s);
    ENTITY_HANDLE h3 = EntityCreate(&s);
    ENTITY_HANDLE h4 = EntityCreate(&s);
    ENTITY_HANDLE h5 = EntityCreate(&s);
    ENTITY_HANDLE h6 = EntityCreate(&s);
    ENTITY_HANDLE h7 = EntityCreate(&s);
    ENTITY_HANDLE h8 = EntityCreate(&s);
    ENTITY_HANDLE h9 = EntityCreate(&s);
    ENTITY_HANDLE h10 = EntityCreate(&s);

    {
        Health health0 = {77};
        Pos pos0 = {.x = 666, .y = 999};
        EntityAddComponent(h0, &s, COMPONENT_POS, &pos0);
        EntityAddComponent(h0, &s, COMPONENT_HEALTH, &health0);
    }

    {
        Health health1 = {12345};
        Pos pos1 = {.x = 111, .y = 222};
        EntityAddComponent(h1, &s, COMPONENT_POS, &pos1);
        EntityAddComponent(h1, &s, COMPONENT_HEALTH, &health1);
    }

    {
        Health health3 = {-99999};
        Pos pos2 = {.x = 3003, .y = 2002};
        EntityAddComponent(h2, &s, COMPONENT_POS, &pos2);
        EntityAddComponent(h2, &s, COMPONENT_HEALTH, &health3);
    }

    {
        Health health10 = {10};
        Pos pos10 = {.x = 10, .y = 10};
        EntityAddComponent(h10, &s, COMPONENT_POS, &pos10);
        EntityAddComponent(h10, &s, COMPONENT_HEALTH, &health10);
    }

    {
        Pos* pPos0 = EntityGetComponent(h0, &s, COMPONENT_POS);
        Health* pHealth0 = EntityGetComponent(h0, &s, COMPONENT_HEALTH);
        Pos* pPos1 = EntityGetComponent(h1, &s, COMPONENT_POS);
        Pos* pPos2 = EntityGetComponent(h2, &s, COMPONENT_POS);

        K_CTX_LOG_DEBUG("pPos0: {:.3:f}, {:.3:f}", pPos0->x, pPos0->y);
        K_CTX_LOG_DEBUG("pHealth0: {i}", pHealth0->val);
        K_CTX_LOG_DEBUG("pPos1: {:.3:f}, {:.3:f}", pPos1->x, pPos1->y);
        K_CTX_LOG_DEBUG("pPos2: {:.3:f}, {:.3:f}", pPos2->x, pPos2->y);
    }

    // K_CTX_LOG_DEBUG("size: {i}", s.size);

    EntityRemove(h1, &s);

    {
        Pos* pPos = ComponentAt(0, &s, COMPONENT_POS);
        Health* pHealth = ComponentAt(0, &s, COMPONENT_HEALTH);
        for (int i = 0; i < s.size; ++i)
        {
            K_CTX_LOG_INFO("{i} pos: ({:.3:f}, {:.3:f}), health: {i}", i, pPos[i].x, pPos[i].y, pHealth[i].val);
        }
    }

    // EntityRemove(h0, &s);
    // EntityRemove(h1, &s);

    ComponentsMapDestroy(&s);
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
