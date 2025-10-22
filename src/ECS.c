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

typedef enum COMPONENT_I
{
    COMPONENT_I_POS,
    COMPONENT_I_HEALTH,
    COMPONENT_I_ESIZE
} COMPONENT_I;

static const ssize_t COMPONENT_SIZES[COMPONENT_I_ESIZE] = {
    sizeof(Pos),
    sizeof(Health),
};

typedef struct ComponentList
{
    uint8_t aList[COMPONENT_I_ESIZE];
    uint8_t size;
} ComponentList;

typedef int ENTITY_HANDLE;
static ENTITY_HANDLE ENTITY_HANDLE_INVALID = -1;

typedef struct ComponentsMap
{
    int cap;
    int size;
    int freeListSize;

    int aDense[100];
    int aSparse[100];
    int aFreeList[100];
    ComponentList aComponentLists[100];

    void* pComponents[COMPONENT_I_ESIZE];
} ComponentsMap;


static void
ComponentsMapDestroy(ComponentsMap* s)
{
    free(s->pComponents[0]);
}

static bool
ComponentsMapGrow(ComponentsMap* s, int newCap)
{
    ssize_t totalCap = 0;
    for (ssize_t i = 0; i < K_ASIZE(COMPONENT_SIZES); ++i)
        totalCap += newCap*COMPONENT_SIZES[i];

    uint8_t* pNew = malloc(totalCap);
    if (!pNew) return false;
    void* pOld = s->pComponents[0];

    for (ssize_t i = 0, off = 0; i < COMPONENT_I_ESIZE; ++i)
    {
        if (s->pComponents[i]) memcpy(pNew + off, s->pComponents[i], s->size);
        s->pComponents[i] = pNew + off;
        off += COMPONENT_SIZES[i]*newCap;
    }

    free(pOld);
    s->cap = newCap;

    return true;
}

static ENTITY_HANDLE
EntityCreate(ComponentsMap* s)
{
    if (s->size >= s->cap)
    {
        if (!ComponentsMapGrow(s, K_MAX(8, s->cap * 2)))
            return ENTITY_HANDLE_INVALID;
    }

    int i;

    if (s->freeListSize > 0) i = s->aFreeList[--s->freeListSize];
    else i = s->size;

    s->aDense[s->size] = i;
    s->aSparse[i] = s->size;
    s->aComponentLists[i].size = 0;

    ++s->size;
    return i;
}

static void
EntityRemove(ENTITY_HANDLE h, ComponentsMap* s)
{
    s->aFreeList[s->freeListSize++] = h;

    const int thisSparseI = h;
    const int thisDenseI = s->aSparse[thisSparseI];
    const int newSparseI = s->aDense[thisDenseI] = s->aDense[s->size - 1];
    const int newDenseI = s->aSparse[newSparseI];

    ComponentList* pThisComp = &s->aComponentLists[thisDenseI];
    ComponentList* pNewComp = &s->aComponentLists[newDenseI];

    for (int i = 0; i < pNewComp->size; ++i)
    {
        COMPONENT_I thisCompI = pThisComp->aList[i] = pNewComp->aList[i];
        const ssize_t thisCompSize = COMPONENT_SIZES[thisCompI];

        memcpy(
            (uint8_t*)s->pComponents[thisCompI] + thisDenseI*thisCompSize,
            (uint8_t*)s->pComponents[thisCompI] + newDenseI*thisCompSize,
            thisCompSize
        );
    }
    pThisComp->size = pNewComp->size;
    s->aSparse[thisSparseI] = newSparseI;
    --s->size;
}

static void
EntityAddComponent(ENTITY_HANDLE h, ComponentsMap* s, COMPONENT_I eComp, void* pVal)
{
    const int dI = s->aSparse[h];
    ComponentList* pCl = &s->aComponentLists[dI];

#ifndef NDEBUG
    for (int i = 0; i < pCl->size; ++i)
        K_ASSERT(pCl->aList[i] != eComp, "must not add same component twice");
#endif

    pCl->aList[pCl->size++] = eComp;
    uint8_t* pComp = s->pComponents[eComp];
    memcpy(pComp + dI*COMPONENT_SIZES[eComp], pVal, COMPONENT_SIZES[eComp]);
}

static void*
EntityGetComponent(ENTITY_HANDLE h, ComponentsMap* s, COMPONENT_I eComp)
{
    return (uint8_t*)s->pComponents[eComp] + s->aSparse[h]*COMPONENT_SIZES[eComp];
}

static void*
ComponentAt(int denseI, ComponentsMap* s, COMPONENT_I eComp)
{
    return (uint8_t*)s->pComponents[eComp] + denseI*COMPONENT_SIZES[eComp];
}

static void
test(void)
{
    ComponentsMap s = {0};

    ENTITY_HANDLE h0 = EntityCreate(&s);
    ENTITY_HANDLE h1 = EntityCreate(&s);
    ENTITY_HANDLE h2 = EntityCreate(&s);

    {
        Health health0 = {77};
        Pos pos0 = {.x = 666, .y = 999};
        EntityAddComponent(h0, &s, COMPONENT_I_POS, &pos0);
        EntityAddComponent(h0, &s, COMPONENT_I_HEALTH, &health0);
    }

    {
        Health health1 = {12345};
        Pos pos1 = {.x = 111, .y = 222};
        EntityAddComponent(h1, &s, COMPONENT_I_POS, &pos1);
        EntityAddComponent(h1, &s, COMPONENT_I_HEALTH, &health1);
    }

    {
        Health health3 = {-99999};
        Pos pos2 = {.x = 3003, .y = 2002};
        EntityAddComponent(h2, &s, COMPONENT_I_POS, &pos2);
        EntityAddComponent(h2, &s, COMPONENT_I_HEALTH, &health3);
    }

    {
        Pos* pPos0 = EntityGetComponent(h0, &s, COMPONENT_I_POS);
        Health* pHealth0 = EntityGetComponent(h0, &s, COMPONENT_I_HEALTH);
        Pos* pPos1 = EntityGetComponent(h1, &s, COMPONENT_I_POS);
        Pos* pPos2 = EntityGetComponent(h2, &s, COMPONENT_I_POS);

        K_CTX_LOG_DEBUG("pPos0: {:.3:f}, {:.3:f}", pPos0->x, pPos0->y);
        K_CTX_LOG_DEBUG("pHealth0: {i}", pHealth0->val);
        K_CTX_LOG_DEBUG("pPos1: {:.3:f}, {:.3:f}", pPos1->x, pPos1->y);
        K_CTX_LOG_DEBUG("pPos2: {:.3:f}, {:.3:f}", pPos2->x, pPos2->y);
    }

    K_CTX_LOG_DEBUG("size: {i}", s.size);

    EntityRemove(h1, &s);

    {
        Pos* pPos = ComponentAt(0, &s, COMPONENT_I_POS);
        Health* pHealth = ComponentAt(0, &s, COMPONENT_I_HEALTH);
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
